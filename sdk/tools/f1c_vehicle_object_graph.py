"""Bounded read-only scene/spatial evidence. Only memory-mcp is a public entry.

Source: 4E16C0 exact-target disassembly, 4E19D0, 54B330/54C030,
54B950, 410980/40F1C0, 40ED10/410B60, 4BDE20/4BD860.
No native calls, cache invalidation, arbitrary-address request or game writes.
"""
import struct, math


def checked_read(read, address, size):
    if not 0x10000<=address<=0xffffffff-size or not 0<size<=65536:
        raise ValueError('Invalid object read bound')
    b=read(address,size)
    if len(b)!=size:raise ValueError('Incomplete object read')
    return b


def u32(b,o=0):return struct.unpack_from('<I',b,o)[0]
def vec(b,o,n=3):
    return [v if math.isfinite(v) else None for v in struct.unpack_from('<'+str(n)+'f',b,o)]


def spatial_inventory(read):
    result={'objects':[], 'cells':[], 'complete':False}
    try:
        manager=checked_read(read,0x5ea560,20)
        count,ptr=u32(manager),u32(manager,8)
        if count>1024:raise ValueError('Object count exceeds1024')
        table=checked_read(read,ptr,4*count) if count else b''
        for i in range(count):
            address=u32(table,4*i)
            h=checked_read(read,address,36)
            item={'address':hex(address),'id':u32(h),'type':u32(h,28),
                  'position':vec(h,12),'radius':vec(h,24,1)[0],
                  'payload':hex(u32(h,32)), 'raw_hex':h.hex()}
            if item['type']<=5 and u32(h,32):
                try:
                    payload=checked_read(read,u32(h,32),0xd4)
                    item['loose_scene_node']=hex(u32(payload,0xd0))
                    item['loose_payload_hex']=payload.hex()
                    item['ownership']='Loose object present; originating driver not established'
                except ValueError as e:item['payload_error']=str(e)
            result['objects'].append(item)
        cell_block=checked_read(read,0x8ab710,65*36)
        total=0
        for i in range(65):
            cell={'cell':i,'lists':[],'members':[]}
            # Native4BDA20 reads lists0/1;4BDB10 reads list2. All three matter.
            for kind in range(3):
                capacity,used,ptr=struct.unpack_from('<III',cell_block,36*i+12*kind)
                if used>capacity or used>1024:raise ValueError('Invalid spatial cell count')
                total+=used
                if total>8192:raise ValueError('Spatial membership bound exceeded')
                data=checked_read(read,ptr,used*4) if used else b''
                members=[hex(u32(data,j*4)) for j in range(used)]
                cell['lists'].append({'kind':kind,'members':members});cell['members']+=members
            result['cells'].append(cell)
        known={o['address'] for o in result['objects']}
        extra=sorted({m for c in result['cells'] for m in c['members']}-known)
        if len(extra)>2048:raise ValueError('Additional spatial object bound exceeded')
        result['additional_cell_objects']=[]
        for address in extra:
            h=checked_read(read,int(address,16),36)
            result['additional_cell_objects'].append({'address':address,'id':u32(h),'type':u32(h,28),
                 'position':vec(h,12),'radius':vec(h,24,1)[0],'payload':hex(u32(h,32)),'raw_hex':h.hex()})
        result['grid_parameters']=vec(checked_read(read,0x8ab710+0x924,16),0,4)
        result['complete']=True
    except ValueError as e:result['error']=str(e)
    return result


def retired_part_pools(read, node_addresses):
    """Native4E0D00 scans these pools by scene-node identity, even after removal."""
    result={'complete':False,'pools':[]}
    try:
        for base,stride in [(0x5ea5a8,808),(0x5ea5b4,792),(0x5ea5c0,792)]:
            count,records,payloads=struct.unpack('<III',checked_read(read,base,12))
            if count>1024:raise ValueError('Part pool count exceeds1024')
            pool={'manager_address':hex(base),'count':count,'payload_stride':stride,'matched':[]}
            for i in range(count):
                address=payloads+i*stride
                scene=u32(checked_read(read,address+208,4))
                if hex(scene) not in node_addresses:continue
                rb=checked_read(read,records+36*i,36)
                pb=checked_read(read,address,stride)
                pool['matched'].append({'slot':i,'record_address':hex(records+36*i),'payload_address':hex(address),
                    'scene_node':hex(scene),'record_id_signed':struct.unpack_from('<i',rb)[0],
                    'record_position':vec(rb,12),'record_raw_hex':rb.hex(),'payload_raw_hex':pb.hex()})
            result['pools'].append(pool)
        result['complete']=True
    except ValueError as e:result['error']=str(e)
    return result


def scene_inventory(read, vehicle_blob):
    # Node pointer in embedded scene pose is +4 (verified instruction4E16D6).
    # Wheel branch pointers come from4E19D0; all offsets below are vehicle-relative.
    # 4DFCE0 also retains detached wing/debris nodes and cockpit/body/lights.
    roots=([0x6f4]+[0x6f0+o+296*i for i in range(4) for o in (324,392)]+
           [0x6f0+4*i for i in [377,394,*range(411,531,17),*range(547,557)]])
    result={'roots':[{ 'vehicle_offset':hex(o),'address':hex(u32(vehicle_blob,o))} for o in roots],
            'nodes':[], 'complete':False, 'errors':[]}
    queue=[(u32(vehicle_blob,o),False) for o in roots if u32(vehicle_blob,o)]
    seen=set(); headers={}; sibling_walked=set()
    while queue:
        address,follow_sibling=queue.pop(0)
        if address in seen:
            # A named wheel root can later be encountered as a chassis child.
            # Still follow its sibling edge, even though its bytes were read.
            if follow_sibling and address not in sibling_walked and address in headers:
                sibling_walked.add(address)
                sibling=u32(headers[address],0x4c)
                if sibling:queue.append((sibling,True))
            continue
        if len(seen)>=128:
            result['errors'].append('128-node per-vehicle traversal bound');break
        seen.add(address)
        try:
            b=checked_read(read,address,0x23c)
            headers[address]=b
            node={'address':hex(address),'raw_hex':b.hex(),'local_xyz':vec(b,0x20),
                  'name':b[:32].split(b'\0')[0].decode('latin1'), 'parent':hex(u32(b,0x40)),
                  'local_euler':vec(b,0x2c),'flags':hex(u32(b,0x3c)),
                  'child':hex(u32(b,0x44)),'sibling':hex(u32(b,0x4c)),
                  'matrix_1a0':vec(b,0x1a0,16), 'render_records':[]}
            # Parent can have several linked render records; preserve all within cap.
            render=u32(b,0x204);renders=set()
            while render:
                if render in renders:raise ValueError('Render record cycle')
                if len(renders)>=32:raise ValueError('Render record bound')
                renders.add(render)
                rb=checked_read(read,render,0xf4)
                rr={'address':hex(render),'raw_hex':rb.hex(),'flags':hex(u32(rb,0x14)),
                    'local_center':vec(rb,0xdc),'cached_world_center':vec(rb,0xe8),
                    'world_cache_valid':bool(u32(rb,0x14)&0x20000000)}
                node['render_records'].append(rr)
                render=u32(rb,0x10)
            result['nodes'].append(node)
            if u32(b,0x44):queue.append((u32(b,0x44),True))
            if follow_sibling:
                sibling_walked.add(address)
                if u32(b,0x4c):queue.append((u32(b,0x4c),True))
        except ValueError as e:
            result['errors'].append(hex(address)+': '+str(e))
    result['complete']=not result['errors']
    result['limitation']='Complete means bounded named scene branches traversed without read errors, not complete ownership of every collider/debris object. Cache values require validity flags.'
    return result

"""Deterministic tests; real elevated IPC is a separate verification step."""
import copy
from pathlib import Path
import sys
import unittest
import struct
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from f1c_memory_mcp import Service, validate, compare, ACCESS, select_process, validate_target, SHA256
from f1c_memory_mcp import position_inventory, session_context, restart_service_inventory
from f1c_vehicle_object_graph import scene_inventory, spatial_inventory, retired_part_pools


class Reader:
    def __init__(self):
        self.data = {'identity': {'pid': 1, 'created': 1}, 'sim_time': 10,
                     'heartbeat': 1, 'drivers': [{'slot': 0, 'name': 'Test', 'speed_ms': 4}]}
        self.error = None
    def capture(self):
        if self.error:
            raise ValueError(self.error)
        return copy.deepcopy(self.data)


class Tests(unittest.TestCase):
    def test_restart_service_read_fixed_regions_and_explicit_candidates(self):
        block=bytearray(0x48c0);block[0x4620:0x4624]=b'Test'
        struct.pack_into('<ff',block,0x3984,34,90)
        struct.pack_into('<i',block,0x397c,2)
        struct.pack_into('<iii',block,0x4070,1,2,10)
        player=bytearray(0x1460);struct.pack_into('<d',player,0x19c,34)
        struct.pack_into('<i',player,5212,2)
        for i in range(4):
            struct.pack_into('<f',block,0x363c+248*i,0.9-i*0.01)
            struct.pack_into('<d',player,652+1124*i+916,0.9-i*0.01)
        def read(a,n):
            self.assertEqual((a,n),(0x7c7428,0x1460));return bytes(player)
        result=restart_service_inventory(read,bytes(block),0x48c0,1)
        self.assertEqual(result['vehicles'][0]['penalty_pairs'][0],[2,10])
        self.assertEqual(result['vehicles'][0]['fuel_capacity'],90)
        self.assertEqual(result['player_physics']['fuel_mirror'],34)
        self.assertAlmostEqual(result['player_physics']['wheels'][3]['condition_candidate'],0.87)
        self.assertEqual(bytes.fromhex(result['player_physics']['raw_hex']),player)
        with self.assertRaises(ValueError):restart_service_inventory(lambda a,n:b'',block,0x48c0,1)
        with self.assertRaises(ValueError):restart_service_inventory(read,b'',0x48c0,1)
        with self.assertRaises(ValueError):restart_service_inventory(read,block,1,1)
        struct.pack_into('<d',player,652+916,float('nan'))
        self.assertIsNone(restart_service_inventory(read,block,0x48c0,1)['player_physics']['wheels'][0]['condition_candidate'])

    def test_native_session_context_exact_width_and_failure(self):
        data={0x7c569c:struct.pack('<i',7),0x7c56a4:struct.pack('<i',7),
              0x7c56a8:struct.pack('<i',5),0x7c56ac:b'\x00',
              0x7ce258:struct.pack('<i',0),0x7c54e8:struct.pack('<I',0x5ec6d8),
              0x5c329c:struct.pack('<i',22),0x8ab6e4:struct.pack('<i',1),
              0x7c54f0:struct.pack('<104I',*range(104))}
        def read(a,n):
            if len(data[a])!=n:raise ValueError('Incomplete context')
            return data[a]
        result=session_context(read)
        self.assertEqual(result['game_state'],7)
        self.assertEqual(result['pause_flag'],0)
        self.assertEqual(result['player_vehicle'],'0x5ec6d8')
        self.assertEqual(result['vehicle_count'],22)
        self.assertEqual(result['replay_mode'],1)
        self.assertEqual(result['active_vehicle_pointers'],[hex(x) for x in range(104)])
        data[0x7ce258]=b'\x00'
        with self.assertRaises(ValueError):session_context(read)

    def test_all_three_cell_lists_include_uncounted_records(self):
        manager=struct.pack('<5I',1,0,0x10000,0,0)
        cells=bytearray(65*36)
        data={0x5ea560:manager,0x10000:struct.pack('<I',0x20000),
              0x20000:bytes(36),0x30000:bytes(36),0x40000:bytes(36),
              0x8ab710+0x924:bytes(16)}
        for kind,address in enumerate((0x20000,0x30000,0x40000)):
            table=0x50000+kind*4
            struct.pack_into('<III',cells,12*kind,1,1,table)
            data[table]=struct.pack('<I',address)
        data[0x8ab710]=bytes(cells)
        def read(a,n):return data[a][:n]
        result=spatial_inventory(read)
        self.assertTrue(result['complete'])
        self.assertEqual([x['address'] for x in result['additional_cell_objects']],['0x30000','0x40000'])
        self.assertEqual(len(result['cells'][0]['lists']),3)
        struct.pack_into('<I',cells,16,2);data[0x8ab710]=bytes(cells)
        self.assertFalse(spatial_inventory(read)['complete'])

    def test_part_pool_retains_inactive_records_and_rejects_excess(self):
        data={0x5ea5a8:struct.pack('<III',1,0x10000,0x20000),
              0x5ea5b4:bytes(12),0x5ea5c0:bytes(12),
              0x20000+208:struct.pack('<I',0x30000),0x20000:bytes(808),
              0x10000:struct.pack('<i',-1)+bytes(32)}
        def read(a,n):return data[a][:n]
        result=retired_part_pools(read,{'0x30000'})
        self.assertTrue(result['complete'])
        self.assertEqual(result['pools'][0]['matched'][0]['record_id_signed'],-1)
        self.assertEqual(len(bytes.fromhex(result['pools'][0]['matched'][0]['payload_raw_hex'])),808)
        data[0x5ea5c0]=struct.pack('<III',1025,0,0)
        self.assertFalse(retired_part_pools(read,{'0x30000'})['complete'])

    def test_scene_and_spatial_missing_or_cyclic_are_explicit(self):
        car=bytearray(0x48c0);struct.pack_into('<I',car,0x6f4,0x10000)
        node=bytearray(0x23c);struct.pack_into('<I',node,0x204,0x20000)
        rb=bytearray(0xf4);struct.pack_into('<I',rb,0x10,0x20000)
        data={0x10000:bytes(node),0x20000:bytes(rb)}
        def read(a,n):
            if a not in data:raise ValueError('Unmapped')
            return data[a][:n]
        self.assertFalse(scene_inventory(read,car)['complete'])
        struct.pack_into('<I',rb,0x10,0);data[0x20000]=bytes(rb)
        result=scene_inventory(read,car)
        self.assertTrue(result['complete'])
        self.assertFalse(result['nodes'][0]['render_records'][0]['world_cache_valid'])
        self.assertFalse(spatial_inventory(read)['complete'])
        # Shared root/child must not truncate a sibling chain (native wheel roots).
        struct.pack_into('<I',car,0x834,0x30000)
        struct.pack_into('<I',node,0x44,0x30000);data[0x10000]=bytes(node)
        child=bytearray(0x23c);struct.pack_into('<I',child,0x4c,0x40000)
        data[0x30000]=bytes(child);data[0x40000]=bytes(0x23c)
        self.assertEqual(len(scene_inventory(read,car)['nodes']),3)
        for request in ({'op':'positions','slot':-1},{'op':'positions','slot':128},{'op':'positions','slot':True},{'op':'drivers','slot':0}):
            with self.assertRaises(ValueError):validate(request)
        validate({'op':'positions','slot':13})

    def test_excluded_retiree_positions_and_raw_coverage(self):
        block=bytearray(0x48c0)
        block[0x4620:0x4624]=b'Kimi'
        struct.pack_into('<i',block,0x32c8,-1)
        struct.pack_into('<i',block,0x4064,2)
        block[0x4410]=1
        struct.pack_into('<3f',block,0x34e0,10,20,30)
        struct.pack_into('<3f',block,0x3b6c,100,20,300)
        data={0x5ea418:struct.pack('<I',0x10000),0x5ea424:struct.pack('<I',1),
              0x10000:struct.pack('<6f',10,20,30,0,0,0)}
        def read(a,n):
            if a not in data:raise ValueError('Unmapped')
            return data[a]
        row=position_inventory(read,bytes(block),0x5ec6d8,0x48c0,1)['rows'][0]
        self.assertEqual(row['body_garage_distance_m'],0)
        self.assertEqual(row['transformed_point_world'],[100,20,300])
        self.assertEqual(bytes.fromhex(row['vehicle_bytes_hex']),block)
        struct.pack_into('<i',block,0x4064,0);block[0x4410]=0
        active=position_inventory(read,bytes(block),0x5ec6d8,0x48c0,1,target_slot=0)['rows'][0]
        self.assertEqual(bytes.fromhex(active['vehicle_bytes_hex']),block)
        data[0x5ea424]=struct.pack('<I',53)
        row=position_inventory(read,bytes(block),0x5ec6d8,0x48c0,1)['rows'][0]
        self.assertIn('garage_unavailable',row)
        with self.assertRaises(ValueError):
            position_inventory(None,b'',0,0x48c0,129)
        validate({'op':'positions'})
        with self.assertRaises(ValueError):
            validate({'op':'positions','address':1})

    def test_rejected_explicit_target_remains_unvalidated(self):
        block=bytearray(0x48c0)
        def no_pointer_read(a,n):
            raise ValueError('Unmapped')
        for name,role in ((b'',-1),(b'Alonso',3)):
            block[0x4620:0x4660]=name.ljust(64,b'\0')
            struct.pack_into('<i',block,0x32c8,role)
            inv=position_inventory(no_pointer_read,bytes(block),0x5ec6d8,0x48c0,1,target_slot=0)
            self.assertEqual(inv['rows'],[])
            self.assertFalse(inv['rejected_target']['validated_driver'])
            self.assertEqual(bytes.fromhex(inv['rejected_target']['vehicle_bytes_hex']),block)
            self.assertNotIn('scene_graph',inv['rejected_target'])
            self.assertIsNone(position_inventory(no_pointer_read,bytes(block),0x5ec6d8,0x48c0,1)['rejected_target'])

    def test_explicit_replay_scene_requires_self_owned_named_identity(self):
        from unittest.mock import patch
        block=bytearray(0x48c0)
        block[0x4620:0x4660]=b'Alonso'.ljust(64,b'\0')
        struct.pack_into('<i',block,0x32c8,3)
        struct.pack_into('<I',block,0,0x5ec6d8)
        def read(a,n): raise ValueError('Unmapped')
        with patch('f1c_vehicle_object_graph.scene_inventory',return_value={'complete':True}) as scene:
            inv=position_inventory(read,bytes(block),0x5ec6d8,0x48c0,1,target_slot=0)
            self.assertFalse(inv['rejected_target']['validated_driver'])
            self.assertEqual(inv['rows'],[])
            self.assertTrue(inv['rejected_target']['replay_scene_graph']['complete'])
            scene.assert_called_once()
            for offset,fmt,value in [(0,'I',0),(0x4898,'i',-1),(0x4898,'i',4096),(0x4064,'i',9),(0x4410,'B',2),(0x45f4,'B',2)]:
                bad=bytearray(block);struct.pack_into('<'+fmt,bad,offset,value);scene.reset_mock()
                inv=position_inventory(read,bytes(bad),0x5ec6d8,0x48c0,1,target_slot=0)
                self.assertNotIn('replay_scene_graph',inv['rejected_target']);scene.assert_not_called()
            scene.reset_mock()
            position_inventory(read,bytes(block),0x5ec6d8,0x48c0,1)
            scene.assert_not_called()

    def test_target_gate(self):
        for candidates in ([], [1, 2]):
            with self.assertRaises(ValueError):
                select_process(candidates)
        self.assertEqual(select_process([12]), 12)
        validate_target(0x400000, SHA256.lower())
        for base, digest in [(0x500000, SHA256), (0x400000, '0'*64)]:
            with self.assertRaises(ValueError):
                validate_target(base, digest)

    def test_allowlist(self):
        for request in ({'op': 'write'}, {'op': 'drivers', 'address': 1}, {}, [],
                        {'op': 'trace', 'samples': True}, {'op': 'trace', 'samples': 11},
                        {'op': 'trace', 'interval': float('nan')}, {'op': 'trace', 'interval': .01}):
            with self.assertRaises(ValueError):
                validate(request)
        self.assertEqual(ACCESS, 0x410)
        validate({'op': 'trace', 'samples': 2, 'interval': .25})

    def test_freshness_reconnect_reset(self):
        reader = Reader(); service = Service(reader)
        self.assertEqual(service.snapshot()['freshness'], 'first_sample_or_new_session')
        self.assertIn('unchanged', service.snapshot()['freshness'])
        reader.data['sim_time'] += 1
        self.assertEqual(service.snapshot()['freshness'], 'advancing')
        reader.data['identity']['created'] = 2  # PID reused
        self.assertEqual(service.snapshot()['freshness'], 'first_sample_or_new_session')
        reader.data['sim_time'] = 0
        self.assertEqual(service.snapshot()['freshness'], 'first_sample_or_new_session')

    def test_failures_clear_previous(self):
        for error in ['Game not found', 'Access denied', 'Unsupported executable', 'Cross tick']:
            reader = Reader(); service = Service(reader); service.snapshot()
            reader.error = error
            result = service.snapshot()
            self.assertFalse(result['ok']); self.assertIsNone(service.previous)
            self.assertIsNone(result['pid']); self.assertEqual(result['freshness'], 'unavailable')
            reader.error = None
            self.assertEqual(service.snapshot()['freshness'], 'first_sample_or_new_session')

    def test_compare_does_not_invent_intentions(self):
        service = Service(Reader()); a = service.snapshot(); b = service.snapshot()
        b['drivers'][0]['name'] = 'New driver'
        self.assertTrue(compare(a,b)['driver_changes'][0]['identity_changed'])
        b['identity']['pid'] = 2
        self.assertFalse(compare(a,b)['comparable'])
        self.assertFalse(compare({},b)['comparable'])


if __name__ == '__main__':
    unittest.main()

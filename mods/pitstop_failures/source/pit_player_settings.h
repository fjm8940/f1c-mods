#pragma once

#include <cerrno>
#include <cstdio>
#include <string>
#include "pit_failure_policy_v3.h"

/* Small player surface over the fully reviewed, embedded v3 policy. Parse a
 * complete file into a copy so an invalid edit never partly changes a stop. */
enum PitPlayerSettingsLoad { PIT_SETTINGS_DEFAULTS, PIT_SETTINGS_LOADED, PIT_SETTINGS_INVALID };

static std::string pit_settings_trim(const std::string &value) {
    const char *spaces=" \t\r\n";
    size_t first=value.find_first_not_of(spaces);
    if(first==std::string::npos)return "";
    size_t last=value.find_last_not_of(spaces);
    return value.substr(first,last-first+1);
}

static bool pit_settings_integer(const std::string &value,int maximum,int *result) {
    if(!result || value.empty() || value.size()>3)return false;
    int parsed=0;
    for(char ch:value) {
        if(ch<'0' || ch>'9')return false;
        parsed=parsed*10+(ch-'0');
        if(parsed>maximum)return false;
    }
    *result=parsed;return true;
}

static bool pit_settings_percent_bp(const std::string &value,int *result) {
    if(!result || value.empty() || value.size()>6)return false;
    size_t dot=value.find('.');
    if(dot!=std::string::npos && value.find('.',dot+1)!=std::string::npos)return false;
    std::string whole=value.substr(0,dot);
    int percent=0;
    if(!pit_settings_integer(whole,100,&percent))return false;
    int fraction=0;
    if(dot!=std::string::npos) {
        std::string decimals=value.substr(dot+1);
        if(decimals.empty() || decimals.size()>2 ||
           !pit_settings_integer(decimals,99,&fraction))return false;
        if(decimals.size()==1)fraction*=10;
    }
    int bp=percent*100+fraction;
    if(bp<PF3_GATE_MIN_BP || bp>PF3_GATE_MAX_BP)return false;
    *result=bp;return true;
}

static bool pit_settings_parse(const std::string &contents,const pf3_config_t &base,
                               pf3_config_t *out,std::string *error) {
    if(!out)return false;
    if(contents.empty() || contents.size()>8192) {
        if(error)*error="INI must contain 1..8192 bytes";
        return false;
    }
    const char *keys[]={"incident_chance_percent","slow_wheel","wheel_gun",
                        "fuel_rig","engine_stall","cross_thread",
        "slow_wheel_minor","slow_wheel_awkward",
        "wheel_gun_quick","wheel_gun_backup","wheel_gun_irreparable",
        "fuel_slow_flow","fuel_delayed_flow","fuel_interrupted_flow","fuel_spill",
        "stall_straightforward","stall_difficult",
        "cross_thread_corrected","cross_thread_lengthy","cross_thread_irreparable",
        "fuel_fire_chance_percent","incident_chance_override_percent"};
    const int sub_parent[]={0,0,1,1,1,2,2,2,2,3,3,4,4,4};
    const int sub_resolution[]={0,1,0,1,2,0,1,2,3,0,1,0,1,2};
    pf3_config_t next=base;
    unsigned seen=0;
    int chance_override=-1;
    bool section=false;
    size_t offset=0;
    int line_number=0;
    while(offset<contents.size()) {
        size_t end=contents.find('\n',offset);
        if(end==std::string::npos)end=contents.size();
        std::string line=contents.substr(offset,end-offset);
        offset=end==contents.size()?end:end+1;
        line_number++;
        if(line_number==1 && line.size()>=3 &&
           (unsigned char)line[0]==0xef && (unsigned char)line[1]==0xbb &&
           (unsigned char)line[2]==0xbf)line.erase(0,3);
        line=pit_settings_trim(line);
        if(line.empty() || line[0]==';' || line[0]=='#')continue;
        if(line[0]=='[') {
            if(line!="[PitFailures]" || section) {
                if(error)*error="line "+std::to_string(line_number)+": unexpected or repeated section";
                return false;
            }
            section=true;continue;
        }
        size_t equals=line.find('=');
        if(!section || equals==std::string::npos || line.find('=',equals+1)!=std::string::npos) {
            if(error)*error="line "+std::to_string(line_number)+": expected key = value under [PitFailures]";
            return false;
        }
        std::string key=pit_settings_trim(line.substr(0,equals));
        std::string value=pit_settings_trim(line.substr(equals+1));
        int index=-1;
        for(int i=0;i<22;i++)if(key==keys[i]){index=i;break;}
        if(index<0 || (seen&(1u<<index))) {
            if(error)*error="line "+std::to_string(line_number)+": unknown or duplicate key "+key;
            return false;
        }
        int parsed=0;
        bool valid=index==21 && value=="inherit"?true:
                   (index==0 || index>=20)?pit_settings_percent_bp(value,&parsed):
                            pit_settings_integer(value,100,&parsed);
        if(!valid) {
            if(error)*error="line "+std::to_string(line_number)+": invalid value for "+key;
            return false;
        }
        if(index==0)next.incident_chance_bp=parsed;
        else if(index<6)next.parent_weight[index-1]=parsed;
        else if(index<20)next.resolution_weight[sub_parent[index-6]][sub_resolution[index-6]]=parsed;
        else if(index==20)next.fire_chance_bp=parsed;
        else if(value!="inherit")chance_override=parsed;
        seen|=1u<<index;
    }
    if(!section || (seen&0x3fu)!=0x3fu) {
        if(error)*error="INI requires [PitFailures] and all six base settings (subtype and fire settings are optional)";
        return false;
    }
    if(chance_override>=0)next.incident_chance_bp=chance_override;
    int total=0;
    for(int i=0;i<PF3_PARENT_COUNT;i++)total+=next.parent_weight[i];
    if(total<=0) {
        if(error)*error="at least one failure type weight must be positive";
        return false;
    }
    char policy_error[256]{};
    if(pf3_config_validate(&next,policy_error,sizeof policy_error)!=PF_CONFIG_OK) {
        if(error)*error=policy_error;
        return false;
    }
    *out=next;return true;
}

static PitPlayerSettingsLoad pit_settings_load_file(const char *path,
                                                    pf3_config_t *config,
                                                    std::string *error) {
    if(!path || !config) {
        if(error)*error="invalid settings path or policy";
        return PIT_SETTINGS_INVALID;
    }
    errno=0;
    FILE *file=std::fopen(path,"rb");
    if(!file) {
        if(errno==ENOENT)return PIT_SETTINGS_DEFAULTS;
        if(error)*error="cannot open PitStopFailures.ini";
        return PIT_SETTINGS_INVALID;
    }
    if(std::fseek(file,0,SEEK_END)!=0) {
        std::fclose(file);if(error)*error="cannot size PitStopFailures.ini";
        return PIT_SETTINGS_INVALID;
    }
    long size=std::ftell(file);
    if(size<=0 || size>8192 || std::fseek(file,0,SEEK_SET)!=0) {
        std::fclose(file);if(error)*error="PitStopFailures.ini must contain 1..8192 bytes";
        return PIT_SETTINGS_INVALID;
    }
    std::string contents((size_t)size,'\0');
    size_t read=std::fread(&contents[0],1,(size_t)size,file);
    std::fclose(file);
    if(read!=(size_t)size) {
        if(error)*error="cannot read PitStopFailures.ini completely";
        return PIT_SETTINGS_INVALID;
    }
    pf3_config_t next{};
    if(!pit_settings_parse(contents,*config,&next,error))return PIT_SETTINGS_INVALID;
    *config=next;return PIT_SETTINGS_LOADED;
}

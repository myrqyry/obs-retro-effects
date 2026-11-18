// Test real source-scoped OBS key registration, routing and serialization.
#include <obs.h>
#include <obs-module.h>
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>
struct Keys {
    obs_source_t* source;
    obs_hotkey_id on=OBS_INVALID_HOTKEY_ID,off=OBS_INVALID_HOTKEY_ID;
    unsigned count=0;
};
static bool find_keys(void* opaque,obs_hotkey_id id,obs_hotkey_t* key){
    auto& k=*static_cast<Keys*>(opaque);
    if(obs_hotkey_get_registerer_type(key)!=OBS_HOTKEY_REGISTERER_SOURCE)return true;
    auto* weak=static_cast<obs_weak_source_t*>(obs_hotkey_get_registerer(key));
    if(!obs_weak_source_references_source(weak,k.source))return true;
    const char* name=obs_hotkey_get_name(key);
    if(!strcmp(name,"effect.enable")){k.on=id;++k.count;}
    if(!strcmp(name,"effect.disable")){k.off=id;++k.count;}
    return true;
}
static Keys keys(obs_source_t* s){Keys k{s};obs_enum_hotkeys(find_keys,&k);return k;}
static void route(obs_hotkey_id id,bool down){obs_hotkey_trigger_routed_callback(id,down);}
static void press(obs_hotkey_id id){route(id,true);route(id,false);}
int main(int argc,char**argv){
    if(argc!=4||!obs_startup("en-US",nullptr,nullptr))return 2;
    obs_video_info v{};v.graphics_module="libobs-opengl";v.fps_num=30;v.fps_den=1;
    v.base_width=v.base_height=v.output_width=v.output_height=64;v.output_format=VIDEO_FORMAT_RGBA;v.scale_type=OBS_SCALE_DISABLE;
    if(obs_reset_video(&v)!=OBS_VIDEO_SUCCESS)return 3;
    obs_module_t* mod=nullptr;
    if(obs_open_module(&mod,argv[1],argv[2])!=MODULE_SUCCESS||!obs_init_module(mod))return 4;
    auto make=[&](const char* name){auto* d=obs_data_create();obs_data_set_bool(d,"sight_use_tracking",true);
        auto* s=obs_source_create(argv[3],name,d,nullptr);obs_data_release(d);return s;};
    auto* parent=obs_scene_create("Hotkey fixture");
    auto* a=make("Effect A");auto* b=make("Effect B");
    if(!a||!b||!parent)return 5;
    auto* root=obs_scene_get_source(parent);obs_source_filter_add(root,a);obs_source_filter_add(root,b);
    auto x=keys(a),y=keys(b);
    bool registered=x.count==2&&y.count==2&&x.on!=y.on&&x.off!=y.off;
    bool separate=false,toggle=false,release=false,saved=false,reloaded=false,removed=false;
    if(registered){
        obs_hotkey_enable_callback_rerouting(true);
        press(x.off);separate=!obs_source_enabled(a)&&obs_source_enabled(b);
        route(x.on,false);release=!obs_source_enabled(a);
        press(x.on);separate&=obs_source_enabled(a)&&obs_source_enabled(b);
        // Both paired callbacks receive the same press, as with one shared
        // binding; only the state-appropriate member may change the state.
        toggle=true;
        for(int i=0;i<2;++i){
            const bool expected=!obs_source_enabled(a);
            route(x.on,true);route(x.off,true);
            // Repeated key-down cannot toggle until released.
            route(x.on,true);route(x.off,true);
            toggle&=obs_source_enabled(a)==expected&&obs_source_enabled(b);
            route(x.on,false);route(x.off,false);
            toggle&=obs_source_enabled(a)==expected;
        }
        obs_key_combination_t combo{};combo.key=OBS_KEY_F9;
        obs_hotkey_load_bindings(x.on,&combo,1);obs_hotkey_load_bindings(x.off,&combo,1);
        auto* data=obs_hotkeys_save_source(a);
        auto* on=obs_data_get_array(data,"effect.enable");auto* off=obs_data_get_array(data,"effect.disable");
        saved=on&&off&&obs_data_array_count(on)==1&&obs_data_array_count(off)==1;
        if(on)obs_data_array_release(on);if(off)obs_data_array_release(off);
        obs_source_set_name(a,"Renamed A");
        auto z=keys(a);saved&=z.on==x.on&&z.off==x.off;
        obs_source_filter_remove(root,a);obs_source_release(a);a=nullptr;
        // Verify the old action disappears when the source lifetime ends.
        for(int i=0;i<100;++i){
            bool found=false;auto cb=[](void* ptr,obs_hotkey_id id,obs_hotkey_t*){auto* q=static_cast<std::pair<obs_hotkey_id,bool>*>(ptr);if(id==q->first)q->second=true;return true;};
            std::pair<obs_hotkey_id,bool> q{x.on,false};obs_enum_hotkeys(cb,&q);found=q.second;
            if(!found){removed=true;break;}
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        a=make("Effect A reloaded");obs_source_filter_add(root,a);obs_hotkeys_load_source(a,data);
        auto* copy=obs_hotkeys_save_source(a);auto* arr=obs_data_get_array(copy,"effect.enable");
        reloaded=arr&&obs_data_array_count(arr)==1&&keys(a).count==2;
        if(arr)obs_data_array_release(arr);obs_data_release(copy);obs_data_release(data);
    }
    printf("EFFECT_HOTKEYS registered=%d independent=%d release=%d toggle=%d saved=%d reloaded=%d removed=%d\n",registered,separate,release,toggle,saved,reloaded,removed);
    if(a){obs_source_filter_remove(root,a);obs_source_release(a);}
    obs_source_filter_remove(root,b);obs_source_release(b);obs_scene_release(parent);obs_shutdown();
    return !(registered&&separate&&release&&toggle&&saved&&reloaded&&removed);
}

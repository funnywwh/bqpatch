#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <vector>
#include <string.h>

void usage(){
    printf("bqpath -d oldfile newfile patchfile\n");
    printf("bqpath -p oldfile newfile patchfile\n");
}
struct MapData{
    unsigned char* data;
    size_t len;
    FILE* file;
};
MapData map(const char* fname){
    MapData data = {0};
    FILE* f = fopen(fname,"rb");
    if(!f){
        return data;
    }
    do{
        int fd = fileno(f);
        size_t flen = 0;
        fseek(f,0,SEEK_END);
        flen = ftell(f);
        data.file = f;
        data.len = flen;
        data.data = (unsigned char*)mmap(nullptr,flen,PROT_READ, MAP_PRIVATE , fd , 0); 
    }while(0);
    return data;
}
struct PatchRecord{
    /**
     * @brief 
     * '+' 添加
     * '-' 删除
     * ‘c’ 复制
     */
    char action;
    size_t pos;
    size_t len;
    unsigned char* data;
};
void unmap(MapData& data){
    munmap(data.data,data.len);
    fclose(data.file);
}
int diff(const char* oldfile,const char* newfile,const char* patchfile){
    MapData oldData,newData,patchData;
    oldData = map(oldfile);
    newData = map(newfile);
    size_t oldpos = 0;
    size_t newpos = 0;
    std::vector<PatchRecord> records;
 
    do{
        bool end_file = false;
        while(!end_file){
            if(oldData.data[oldpos] != newData.data[newpos]){
                if(newpos <= oldpos){
                    //增加部分
                    size_t len = 1;
                    size_t new_pre_pos = newpos;
                    size_t insertpos = oldpos;
                    do{
                        oldpos ++;
                        newpos ++;
                        if(oldpos >= oldData.len || newpos >= newData.len){
                            end_file = true;
                            break;
                        }
                        if(oldData.data[oldpos] == newData.data[newpos]){
                            break;
                        }
                        len ++;
                    }while(true);
                    PatchRecord record;
                    record.action = '+';
                    record.len = len;
                    record.data = &newData.data[new_pre_pos];
                    record.pos = insertpos;
                    records.push_back(record);
                }else{
                    //减少部分
                    size_t len = 1;
                    size_t delpos = oldpos;
                    size_t new_pre_pos = newpos;
                    do{
                        oldpos ++;
                        newpos ++;
                        if(oldpos >= oldData.len || newpos >= newData.len){
                            end_file = true;
                            break;
                        }
                        if(oldData.data[oldpos] == newData.data[newpos]){
                            break;
                        }
                        len ++;
                    }while(true);
                    PatchRecord record;
                    record.action = '-';
                    record.len = len;
                    record.pos = delpos;
                    records.push_back(record);
                }
            }else{
                //相等部分
                size_t len = 1;
                size_t copypos = oldpos;
                do{
                    oldpos ++;
                    newpos ++;
                    if(oldpos >= oldData.len || newpos >= newData.len){
                        end_file = true;
                        break;
                    }
                    if(oldData.data[oldpos] != newData.data[newpos]){
                        break;
                    }
                    len ++;
                }while(true);
                PatchRecord record;
                record.action = 'c';
                record.len = len;
                record.pos = copypos;
                records.push_back(record);
            }
            end_file = oldpos >= oldData.len || newpos >= newData.len;
        }

        if(oldpos >= oldData.len){
            if(newpos < newData.len){
                PatchRecord record;
                record.action = '+';
                record.len = newData.len - newpos;
                record.data = &newData.data[newpos];
                record.pos = newpos;
                records.push_back(record);
            }
        }else{
            if(oldpos < oldData.len){
                PatchRecord record;
                record.action = '-';
                record.len = oldData.len - oldpos;
                record.pos = oldpos;
                records.push_back(record);
            }
        }

    }while(0);
    FILE* fpatch = fopen(patchfile,"wb+");
    
    if(fpatch){
        size_t patch_size = 0;
        for(auto record : records){
            unsigned char* data = record.data;
            record.data = nullptr;
            fwrite(&record,sizeof(record),1,fpatch);
            if(record.action == 'c'){
                patch_size += record.len;
            }else if(record.action == '+'){
                patch_size += record.len;
                fwrite(data,record.len,1,fpatch);
            }else if(record.action == '-'){
                patch_size -= record.len;
            }
        }
        printf("patch size:%llu\n",patch_size);
        fclose(fpatch);
    }
    unmap(oldData);
    unmap(newData);
    return 0;
}
int patch(const char* oldfile,const char* newfile,const char* patchfile){
    return 0;
}
int main(int argc,char* argv[]){
    if(argc < 5){
        usage();
        return -1;
    }
    char* oldfile = argv[2];
    char* newfile = argv[3];
    char* patchfile = argv[4];
    printf("oldfile:%s,newfile:%s,patchfile:%s\n",oldfile,newfile,patchfile);
    if(strcmp(argv[1],"-d") == 0){
        return diff(oldfile,newfile,patchfile);
    }else if(strcmp(argv[1],"-p") == 0){
        return patch(oldfile,newfile,patchfile);
    }
    return -1;
}
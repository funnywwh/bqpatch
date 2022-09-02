#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <vector>
#include <string.h>

#if DEBUG
#define LOG(fmt,argv...) printf(fmt,##argv);
#define LOGBIN(data,len) \
{ \
    printf("0x"); \
    for(int i = 0;i<len;i++){ \
        printf("%02x",data[i]); \
    } \
    printf("\n"); \
}\

#else
#define LOG(fmt,argv...)
#define LOGBIN(data,len)
#endif //DDBUG
void usage(){
    printf("bqpath diff oldfile newfile patchfile\n");
    printf("bqpath patch oldfile newfile patchfile\n");
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

enum ACTION{
    ACTION_SIZE = 0x1,
    ACTION_ADD  = 0x2,
    ACTION_DEL  = 0x3,
    ACTION_COPY = 0x4,
};
struct PatchRecord{
    /**
     * @brief 
     * ACTION_SIZE 文件大小
     * ACTION_ADD 添加
     * ACTION_DEL 删除
     * ‘c’ 复制
     */
    unsigned char action;
    size_t len;
    unsigned char* data;
};
void unmap(MapData& data){
    munmap(data.data,data.len);
    fclose(data.file);
}

/**
 * @brief 
 * (MSB)<lensize>(4bit)<action>(4bit)+[len](lensize_byte)+[data](len)
 * @param record 
 * @param fout 
 * @return int 
 */
int write_patch_record(PatchRecord& record,FILE* fout){
    unsigned char action = record.action;
    unsigned lensize = 0;
    int len = record.len;
    char out[5] = {0};
    out[0] = action;

    //little endian
    for(int i = 0;i<4 && len >0;i++){
        out[i+1] = len & 0xff;
        len >>= 8;
        lensize ++;
    }
    out[0] |= lensize << 4;
    int ret = fwrite(out,1+lensize,1,fout);
    if(ret <= 0){
        return -1;
    }
    switch (record.action)
    {
    case ACTION_ADD:
        {
            ret = fwrite(record.data,record.len,1,fout);
            LOG("+,%llu\n",record.len);
            LOGBIN(record.data,record.len);
        }
        break;
    case ACTION_COPY:
        {
            LOG("c,%llu\n",record.len);
            LOGBIN(record.data,record.len);
        };
        break;
    case ACTION_DEL:
        {
            LOG("-,%llu\n",record.len);
        }
        break;
    case ACTION_SIZE:
        {
            LOG("new file size:%llu\n",record.len);
        }
        break;
    default:
        break;
    }
    return 0;
}

int read_patch_record(MapData& patchData,size_t& patchpos,MapData& oldData,size_t& oldpos,PatchRecord& out){
    unsigned char action = patchData.data[patchpos++];
    out.action = action & 0xf;
    out.len = 0;
    int lensize = (action >> 4) & 0xf;
    //little endia
    for(int i = 0;i<lensize;i++){
        out.len |= (patchData.data[patchpos++])<<(i*8);
    }
    switch (out.action)
    {
    case ACTION_ADD:
        {
            out.data = &patchData.data[patchpos];
            patchpos += out.len;
            LOG("+,%llu\n",out.len);
            LOGBIN(out.data,out.len);
        }
        break;
    case ACTION_COPY:
        {
            out.data = &oldData.data[oldpos];
            oldpos += out.len;
            LOG("c,%llu\n",out.len);
            LOGBIN(out.data,out.len);
        }break;
    case ACTION_SIZE:
        {
            LOG("new file size:%llu\n",out.len);
        }
        break;
    case ACTION_DEL:
        {
            oldpos += out.len;
            LOG("-,%llu\n",out.len);
        }
        break;
    default:
        break;
    }
    return 0;
}

int diff(const char* oldfile,const char* newfile,const char* patchfile){
    MapData oldData,newData;
    oldData = map(oldfile);
    newData = map(newfile);
    size_t oldpos = 0;
    size_t newpos = 0;
    std::vector<PatchRecord> records;
 
    PatchRecord sizeRecord= {0};
    sizeRecord.action = ACTION_SIZE;
    sizeRecord.len = newData.len;
    records.push_back(sizeRecord);
    do{
        bool end_file = oldpos >= oldData.len || newpos >= newData.len;
        while(!end_file){
            if(oldData.data[oldpos] != newData.data[newpos]){
                if(newpos <= oldpos){
                    //增加部分
                    size_t len = 1;
                    size_t new_pre_pos = newpos;
                    do{
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
                    record.action = ACTION_ADD;
                    record.len = len;
                    record.data = &newData.data[new_pre_pos];
                    records.push_back(record);
                }else{
                    //减少部分
                    size_t len = 1;
                    size_t new_pre_pos = newpos;
                    do{
                        oldpos ++;
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
                    record.action = ACTION_DEL;
                    record.len = len;
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
                record.action = ACTION_COPY;
                record.len = len;
                record.data = &oldData.data[copypos];
                records.push_back(record);
            }
            end_file = oldpos >= oldData.len || newpos >= newData.len;
        }

        if(oldpos >= oldData.len){
            if(newpos < newData.len){
                PatchRecord record;
                record.action = ACTION_ADD;
                record.len = newData.len - newpos;
                record.data = &newData.data[newpos];
                records.push_back(record);
            }
        }else{
            if(oldpos < oldData.len){
                PatchRecord record;
                record.action = ACTION_DEL;
                record.len = oldData.len - oldpos;
                records.push_back(record);
            }
        }

    }while(0);
    FILE* fpatch = fopen(patchfile,"wb+");
    
    int ret = 0;
    if(fpatch){
        size_t patch_size = 0;
        for(auto record : records){
            int ret = write_patch_record(record,fpatch);
            if(ret < 0 ){
                ret = -1;
                break;
            }
        }
        fclose(fpatch);
    }
    unmap(oldData);
    unmap(newData);
    return ret;
}
int patch(const char* oldfile,const char* newfile,const char* patchfile){
    MapData oldData,newData,patchData;
    oldData = map(oldfile);
    patchData = map(patchfile);
    if(patchData.data == nullptr){
        if(oldData.data != nullptr){
            unmap(oldData);
        }
        return -1;
    }
    size_t patchpos = 0;
    size_t oldpos = 0;
    PatchRecord record;
    FILE* newf = fopen(newfile,"wb+");
    if(!newf){
        return -2;
    }
    do{
        while(patchpos < patchData.len){
            int ret = read_patch_record(patchData,patchpos,oldData,oldpos,record);
            switch (record.action)
            {
            case ACTION_ADD:
                {
                    fwrite(record.data,record.len,1,newf);
                }
                break;
            case ACTION_DEL:
                {
                }
                break;
            case ACTION_COPY:
                {
                    fwrite(record.data,record.len,1,newf);
                }
                break;
            default:
                break;
            }
        }
    }while(0);
    fclose(newf);
    unmap(oldData);
    unmap(patchData);
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
    if(strcmp(argv[1],"diff") == 0){
        return diff(oldfile,newfile,patchfile);
    }else if(strcmp(argv[1],"patch") == 0){
        return patch(oldfile,newfile,patchfile);
    }
    usage();
    return -1;
}
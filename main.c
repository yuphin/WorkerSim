#include <stdio.h>
#include "writeOutput.h"
#include <semaphore.h>
#include <zconf.h>
#include <stdlib.h>




typedef struct Miner{
    unsigned int ID;
    OreType oreType;
    unsigned int capacity;
    unsigned int current_count;
    unsigned int active;
    unsigned int reserved_count;
}Miner;

typedef struct Transporter{
    unsigned int ID;
    OreType *carry;

}Transporter;

typedef struct Smelter{
    unsigned int ID;
    OreType oreType;
    unsigned int loading_capacity;
    unsigned int waiting_ore_count;
    unsigned int total_produce;
    unsigned int available;

}Smelter;
typedef struct Foundry{
    unsigned int ID;
    unsigned int loading_capacity;
    unsigned int waiting_iron;
    unsigned int waiting_coal;
    unsigned int total_produce;

}Foundry;
typedef struct MinerArgStruct{
    Miner* miner;
    unsigned  int ID;
    OreType oreType;
    unsigned  int capacity;
    unsigned  int interval;
    unsigned int totalOre;
}MinerArgStruct;

typedef struct TransporterArgStruct{
    unsigned int ID;
    unsigned int interval;
    Transporter* transporter;
    Miner* miners;
    Smelter* smelters;
    Foundry* foundries;

}TransporterArgStruct;

typedef struct SmelterArgStruct{
    Smelter* smelter;
    unsigned int interval;
}SmelterArgStruct;

typedef struct FoundryArgStruct{
    Foundry* foundry;
    unsigned int interval;
}FoundryArgStruct;

Transporter transEx;
Miner minerEx;
Smelter smelterEx;
Foundry foundryEx;

sem_t minerSem;
sem_t minerMutex;
sem_t activeSem;

sem_t smelterSem;
sem_t smelterMutex;

sem_t foundryMutex;

pthread_cond_t twoOres = PTHREAD_COND_INITIALIZER;
pthread_cond_t minerProduced = PTHREAD_COND_INITIALIZER;
pthread_cond_t storageAvail = PTHREAD_COND_INITIALIZER;
pthread_cond_t oneIronOneCoal = PTHREAD_COND_INITIALIZER;

pthread_mutex_t condMutex_miner = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t condMutex_smelter = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t condMutex_foundry = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t condMutex_smelterStorage = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t minerActiveMutex = PTHREAD_MUTEX_INITIALIZER;
struct timeval tv_smelter;
struct timespec ts_smelter;
struct timeval tv_foundry;
struct timespec ts_foundry;
void transporter_miner(unsigned int ID,unsigned int interval,Miner* miner,OreType*carriedOre){
    MinerInfo mInfo;
    TransporterInfo tInfo;
    FillMinerInfo(&mInfo,miner->ID,0,0,0);

    FillTransporterInfo(&tInfo,ID,NULL);
    WriteOutput(&mInfo,&tInfo,NULL,NULL,TRANSPORTER_TRAVEL);
    usleep(interval - (interval*0.01) + (rand()%(int)(interval*0.02)));

    sem_wait(&minerMutex);
    (miner->current_count)--;
    sem_post(&minerMutex);

    *carriedOre = miner->oreType;

    sem_wait(&minerMutex);
    FillMinerInfo(&mInfo,miner->ID,miner->oreType,miner->capacity,miner->current_count);
    sem_post(&minerMutex);

    FillTransporterInfo(&tInfo,ID,carriedOre);
    WriteOutput(&mInfo,&tInfo,NULL,NULL,TRANSPORTER_TAKE_ORE);
    usleep(interval - (interval*0.01) + (rand()%(int)(interval*0.02)));

    sem_post(&minerSem);




}
void transporter_smelter(unsigned int ID,unsigned int interval,Smelter* smelter,OreType*carriedOre){
    SmelterInfo sInfo;
    TransporterInfo tInfo;
    FillSmelterInfo(&sInfo,smelter->ID,smelter->oreType,0,0,0);
    FillTransporterInfo(&tInfo,ID,carriedOre);
    WriteOutput(NULL,&tInfo,&sInfo,NULL,TRANSPORTER_TRAVEL);
    usleep(interval - (interval*0.01) + (rand()%(int)(interval*0.02)));

    sem_wait(&smelterMutex);
    smelter->waiting_ore_count +=1;
    FillSmelterInfo(&sInfo,smelter->ID,smelter->oreType,smelter->loading_capacity,
                    smelter->waiting_ore_count,smelter->total_produce);
    sem_post(&smelterMutex);

    FillTransporterInfo(&tInfo,ID,carriedOre);
    WriteOutput(NULL,&tInfo,&sInfo,NULL,TRANSPORTER_DROP_ORE);
    usleep(interval - (interval*0.01) + (rand()%(int)(interval*0.02)));

    sem_wait(&smelterMutex);
    if(smelter->waiting_ore_count >=2){
        pthread_mutex_lock(&condMutex_smelter);
        pthread_cond_signal(&twoOres);
        pthread_mutex_unlock(&condMutex_smelter);
    }
    sem_post(&smelterMutex);



}

void transporter_foundry(unsigned int ID,unsigned int interval,Foundry* foundry,OreType*carriedOre){
    FoundryInfo fInfo;
    TransporterInfo tInfo;
    FillFoundryInfo(&fInfo,foundry->ID,0,0,0,0);
    FillTransporterInfo(&tInfo,ID,carriedOre);
    WriteOutput(NULL,&tInfo,NULL,&fInfo,TRANSPORTER_TRAVEL);
    usleep(interval - (interval*0.01) + (rand()%(int)(interval*0.02)));
    if(*carriedOre == IRON){
        sem_wait(&foundryMutex);
        foundry->waiting_iron +=1;
    }else{
        sem_wait(&foundryMutex);
        foundry->waiting_coal +=1;

    }
    FillFoundryInfo(&fInfo,foundry->ID,foundry->loading_capacity,
            foundry->waiting_iron,
            foundry->waiting_coal,foundry->total_produce);
    sem_post(&foundryMutex);
    FillTransporterInfo(&tInfo,ID,carriedOre);
    WriteOutput(NULL,&tInfo,NULL,&fInfo,TRANSPORTER_DROP_ORE);
    usleep(interval - (interval*0.01) + (rand()%(int)(interval*0.02)));

    sem_wait(&foundryMutex);
    if(foundry->waiting_iron && foundry->waiting_coal){
        pthread_mutex_lock(&condMutex_foundry);
        pthread_cond_signal(&oneIronOneCoal);
        pthread_mutex_unlock(&condMutex_foundry);

    }
    sem_post(&foundryMutex);

}

Miner* waitNextLoad(){
    return &minerEx;
}
void* transporter_subroutine(void* transporterArgs){
    TransporterArgStruct args = *(TransporterArgStruct*)transporterArgs;
    TransporterInfo tInfo;
    FillTransporterInfo(&tInfo,args.ID,NULL);
    WriteOutput(NULL,&tInfo,NULL,NULL,TRANSPORTER_CREATED);
    while(1){
        OreType carriedOre = -1;
        pthread_mutex_lock(&minerActiveMutex);
        Miner * minerEx = waitNextLoad();
        if(!minerEx->active && minerEx->reserved_count){
            minerEx->reserved_count --;
            pthread_mutex_unlock(&minerActiveMutex);

        }else if(minerEx->active && !minerEx->reserved_count){

            pthread_cond_wait(&minerProduced,&minerActiveMutex);
            minerEx->reserved_count--;
            pthread_mutex_unlock(&minerActiveMutex);

        }else if(minerEx->active){
            minerEx->reserved_count--;
            pthread_mutex_unlock(&minerActiveMutex);
        }else{
            pthread_mutex_unlock(&minerActiveMutex);
            break;
        }

        transporter_miner(args.ID,args.interval,minerEx,&carriedOre);

        pthread_mutex_lock(&condMutex_smelterStorage);

        while(smelterEx.available ==0)
            pthread_cond_wait(&storageAvail,&condMutex_smelterStorage);
        smelterEx.available -= 1;
        pthread_mutex_unlock(&condMutex_smelterStorage);
        transporter_smelter(args.ID,args.interval,&smelterEx,&carriedOre);

        //usleep(args.interval - (args.interval*0.01) + (rand()%(int)(args.interval*0.02)));


    }

}

void* miner_subroutine(void* minerArgs){
    MinerArgStruct args = *(MinerArgStruct *)minerArgs;
    MinerInfo mInfo;
    sem_wait(&minerMutex);
    args.miner->current_count = 0;
    FillMinerInfo(&mInfo,args.ID,args.oreType,args.capacity,args.miner->current_count);
    sem_post(&minerMutex);
    WriteOutput(&mInfo,NULL,NULL,NULL,MINER_CREATED);

    while(args.totalOre > 0){
        sem_wait(&minerSem);

        sem_wait(&minerMutex);
        FillMinerInfo(&mInfo,args.ID,args.oreType,args.capacity,args.miner->current_count);
        sem_post(&minerMutex);

        WriteOutput(&mInfo,NULL,NULL,NULL,MINER_STARTED);
        usleep(args.interval - (args.interval*0.01) + (rand()%(int)(args.interval*0.02)));

        sem_wait(&minerMutex);
        (args.miner->current_count)++;
        FillMinerInfo(&mInfo,args.ID,args.oreType,args.capacity,args.miner->current_count);
        sem_post(&minerMutex);

        pthread_mutex_lock(&minerActiveMutex);
        (args.miner->reserved_count)++;
        pthread_cond_signal(&minerProduced);
        pthread_mutex_unlock(&minerActiveMutex);

        WriteOutput(&mInfo,NULL,NULL,NULL,MINER_FINISHED);
        usleep(args.interval - (args.interval*0.01) + (rand()%(int)(args.interval*0.02)));

        args.totalOre--;

    }
    // Miner stopped semaphore here
    pthread_mutex_lock(&minerActiveMutex);
    args.miner->active =0;
    pthread_mutex_unlock(&minerActiveMutex);

    sem_wait(&minerMutex);
    FillMinerInfo(&mInfo,args.ID,args.oreType,args.capacity,args.miner->current_count);
    sem_post(&minerMutex);

    WriteOutput(&mInfo,NULL,NULL,NULL,MINER_STOPPED);

}

void* smelter_subroutine(void* smelterArgs){
    SmelterInfo sInfo;
    SmelterArgStruct args = *(SmelterArgStruct*)smelterArgs;

    sem_wait(&smelterMutex);
    FillSmelterInfo(&sInfo,args.smelter->ID,args.smelter->oreType,
                    args.smelter->loading_capacity,
                    args.smelter->waiting_ore_count,args.smelter->total_produce);
    sem_post(&smelterMutex);

    WriteOutput(NULL,NULL,&sInfo,NULL,SMELTER_CREATED);
    while(1){


        sem_wait(&smelterMutex);
        if(args.smelter->waiting_ore_count < 2){
            sem_post(&smelterMutex);
            gettimeofday(&tv_smelter, NULL);
            ts_smelter.tv_sec = time(NULL) + 5;
            ts_smelter.tv_sec += ts_smelter.tv_nsec / (1000 * 1000 * 1000);
            ts_smelter.tv_nsec %= (1000 * 1000 * 1000);
            pthread_mutex_lock(&condMutex_smelter);
            pthread_cond_timedwait(&twoOres,&condMutex_smelter,&ts_smelter);
            pthread_mutex_unlock(&condMutex_smelter);
            sem_wait(&smelterMutex);
            if(args.smelter->waiting_ore_count < 2){
                sem_post(&smelterMutex);
                break;
            }
        }
        args.smelter->waiting_ore_count -= 2;

        FillSmelterInfo(&sInfo,args.smelter->ID,args.smelter->oreType,
                        args.smelter->loading_capacity,args.smelter->waiting_ore_count,args.smelter->total_produce);
        sem_post(&smelterMutex);

        WriteOutput(NULL,NULL,&sInfo,NULL,SMELTER_STARTED);
        usleep(args.interval - (args.interval*0.01) + (rand()%(int)(args.interval*0.02)));

        // Smelter produced
        sem_wait(&smelterMutex);
        args.smelter->total_produce += 1;
        FillSmelterInfo(&sInfo,args.smelter->ID,args.smelter->oreType,
                        args.smelter->loading_capacity,args.smelter->waiting_ore_count,args.smelter->total_produce);
        sem_post(&smelterMutex);

        pthread_mutex_lock(&condMutex_smelterStorage);
        args.smelter->available += 2;
        pthread_cond_signal(&storageAvail);
        pthread_mutex_unlock(&condMutex_smelterStorage);


        WriteOutput(NULL,NULL,&sInfo,NULL,SMELTER_FINISHED);
    }
    //Smelter stopped
    sem_wait(&smelterMutex);
    FillSmelterInfo(&sInfo,args.smelter->ID,args.smelter->oreType,
                    args.smelter->loading_capacity,args.smelter->waiting_ore_count,args.smelter->total_produce);
    sem_post(&smelterMutex);
    WriteOutput(NULL,NULL,&sInfo,NULL,SMELTER_STOPPED);

}

void* foundry_subroutine(void* foundryArgs){
    FoundryInfo fInfo;
    FoundryArgStruct args = *(FoundryArgStruct*)foundryArgs;

    sem_wait(&foundryMutex);
    FillFoundryInfo(&fInfo,args.foundry->ID,args.foundry->loading_capacity,
                    args.foundry->waiting_iron,args.foundry->waiting_coal,
                    args.foundry->total_produce);
    sem_post(&foundryMutex);
    WriteOutput(NULL,NULL,NULL,&fInfo,FOUNDRY_CREATED);
    while(1){


        sem_wait(&foundryMutex);
        if(!args.foundry->waiting_iron || !args.foundry->waiting_coal){
            sem_post(&foundryMutex);
            gettimeofday(&tv_foundry, NULL);
            ts_foundry.tv_sec = time(NULL) + 5;
            ts_foundry.tv_sec += ts_foundry.tv_nsec / (1000 * 1000 * 1000);
            ts_foundry.tv_nsec %= (1000 * 1000 * 1000);
            pthread_mutex_lock(&condMutex_foundry);
            pthread_cond_timedwait(&oneIronOneCoal,&condMutex_foundry,&ts_foundry);
            pthread_mutex_unlock(&condMutex_foundry);
            sem_wait(&foundryMutex);
            if(!args.foundry->waiting_iron || !args.foundry->waiting_coal){
                sem_post(&smelterMutex);
                break;
            }
        }
        args.foundry->waiting_coal-=1;
        args.foundry->waiting_iron-=1;
        FillFoundryInfo(&fInfo,args.foundry->ID,args.foundry->loading_capacity,
                        args.foundry->waiting_iron,args.foundry->waiting_coal,
                        args.foundry->total_produce);
        sem_post(&foundryMutex);
        WriteOutput(NULL,NULL,NULL,&fInfo,FOUNDRY_STARTED);
        usleep(args.interval - (args.interval*0.01) + (rand()%(int)(args.interval*0.02)));

        //Foundry produced
        sem_wait(&foundryMutex);
        args.foundry->total_produce+=1;
        FillFoundryInfo(&fInfo,args.foundry->ID,args.foundry->loading_capacity,
                        args.foundry->waiting_iron,args.foundry->waiting_coal,
                        args.foundry->total_produce);
        sem_post(&foundryMutex);

        //TODO: Storage code here...

        WriteOutput(NULL,NULL,NULL,&fInfo,FOUNDRY_STARTED);






    }




}

int main() {
    time_t t;
    srand((unsigned) time(&t));
    InitWriteOutput();

    sem_init(&minerSem,0,10);
    sem_init(&activeSem,0,1);
    sem_init(&minerMutex,0,1);
    sem_init(&smelterMutex,0,1);
    sem_init(&smelterSem,0,4);
    sem_init(&foundryMutex,0,1);

    pthread_t minerID,transporterID,smelterID,foundryID;
    minerEx = (Miner){1,COPPER,10,0,1,0};
    transEx = (Transporter){1,NULL};
    smelterEx = (Smelter){1,COPPER,8,0,0,8};
    foundryEx = (Foundry){1,4,0,0,0};

    MinerArgStruct minerArgs = {&minerEx,1,COPPER,10,65,30};
    TransporterArgStruct transporterArgs ={1,50,&transEx,&minerEx,NULL,NULL};
    SmelterArgStruct smelterArgs = {&smelterEx,60};

    pthread_create(&transporterID, NULL, transporter_subroutine, (void*)&transporterArgs);
    pthread_create(&minerID, NULL, miner_subroutine,(void*)&minerArgs);
    pthread_create(&smelterID, NULL, smelter_subroutine, (void*)&smelterArgs);
    //pthread_create(&foundryID, NULL, foundry_subroutine, NULL);

    pthread_join(minerID,NULL);
    pthread_join(transporterID,NULL);
    pthread_join(smelterID,NULL);
    //pthread_join(foundryID,NULL);

    sem_destroy(&minerSem);
    sem_destroy(&minerMutex);
    sem_destroy(&activeSem);
    return 0;
}
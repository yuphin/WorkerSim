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
    unsigned int interval;
    unsigned int totalOre;
    pthread_cond_t minerProduced;
    pthread_mutex_t condMutex_miner;
    pthread_mutex_t minerActiveMutex;
    sem_t minerSem;
    sem_t minerMutex;

}Miner;
typedef struct Smelter{
    unsigned int ID;
    unsigned int interval;
    OreType oreType;
    unsigned int loading_capacity;
    unsigned int waiting_ore_count;
    unsigned int total_produce;
    unsigned int available;
    sem_t smelterMutex;
    pthread_cond_t twoOres;
    pthread_cond_t storageAvail;
    pthread_mutex_t condMutex_smelter;
    pthread_mutex_t condMutex_smelterStorage;
    struct timeval tv_smelter;
    struct timespec ts_smelter;
    unsigned int active;

}Smelter;
typedef struct Foundry{
    unsigned int ID;
    unsigned int interval;
    unsigned int loading_capacity;
    unsigned int waiting_iron;
    unsigned int waiting_coal;
    unsigned int total_produce;
    unsigned int available;
    sem_t foundryMutex;
    pthread_cond_t oneIronOneCoal;
    pthread_cond_t storageAvail;
    pthread_mutex_t condMutex_foundryStroage;
    pthread_mutex_t condMutex_foundry;
    struct timeval tv_foundry;
    struct timespec ts_foundry;
    unsigned int active;

}Foundry;
typedef struct Transporter{
    unsigned int ID;
    unsigned int interval;
    Miner* miners;
    Smelter* smelters;
    Foundry* foundries;


}Transporter;

pthread_cond_t producerFinished = PTHREAD_COND_INITIALIZER;
pthread_mutex_t producerMutex = PTHREAD_MUTEX_INITIALIZER;

void init_miner(Miner* miner,unsigned int ID,
                OreType oreType,unsigned int capacity, unsigned int interval, unsigned int totalOre){
    pthread_mutex_init(&miner->minerActiveMutex,NULL);
    pthread_mutex_init(&miner->condMutex_miner,NULL);
    pthread_cond_init(&miner->minerProduced,NULL);

    sem_init(&miner->minerSem,0,capacity);
    sem_init(&miner->minerMutex,0,1);
    miner->ID = ID;
    miner->oreType = oreType;
    miner->capacity = capacity;
    miner->current_count = 0;
    miner->active = 1;
    miner->reserved_count = 0;
    miner->interval = interval;
    miner-> totalOre = totalOre;
}
void destroy_miner(Miner* miner){
    sem_destroy(&miner->minerSem);
    sem_destroy(&miner->minerMutex);
    pthread_cond_destroy(&miner->minerProduced);
    pthread_mutex_destroy(&miner->minerActiveMutex);
    pthread_mutex_destroy(&miner->condMutex_miner);


}

void init_transporter(Transporter* transporter, unsigned int ID, unsigned int interval,
                      Miner* miners, Smelter* smelters, Foundry* foundries){
    transporter->ID = ID;
    transporter->interval = interval;
    transporter->miners = miners;
    transporter->smelters = smelters;
    transporter->foundries = foundries;

}
void destroy_transporter(Transporter* transporter){

}

void init_smelter(Smelter* smelter,unsigned int ID, OreType oreType,
                  unsigned int loading_capacity, unsigned int interval){
    smelter->ID = ID;
    smelter->interval = interval;
    smelter->oreType = oreType;
    smelter->loading_capacity = loading_capacity;
    smelter->waiting_ore_count = 0;
    smelter->total_produce = 0;
    smelter->available = loading_capacity;
    smelter->tv_smelter = (struct timeval){0};
    smelter->ts_smelter = (struct timespec){0};
    pthread_mutex_init(&smelter->condMutex_smelter,NULL);
    pthread_mutex_init(&smelter->condMutex_smelterStorage,NULL);
    pthread_cond_init(&smelter->twoOres,NULL);
    pthread_cond_init(&smelter->storageAvail,NULL);
    sem_init(&smelter->smelterMutex,0,1);
    smelter->active = 1;

}

void destroy_smelter(Smelter* smelter){
    pthread_mutex_destroy(&smelter->condMutex_smelter);
    pthread_mutex_destroy(&smelter->condMutex_smelterStorage);
    pthread_cond_destroy(&smelter->twoOres);
    pthread_cond_destroy(&smelter->storageAvail);
    sem_destroy(&smelter->smelterMutex);

}

void init_foundry(Foundry* foundry, unsigned int ID,
                  unsigned int loading_capacity, unsigned int interval){
    foundry->ID = ID;
    foundry->loading_capacity = loading_capacity;
    foundry->interval = interval;
    foundry->waiting_coal =0;
    foundry->waiting_iron =0;
    foundry->total_produce = 0;
    foundry->tv_foundry = (struct timeval){0};
    foundry->ts_foundry = (struct timespec){0};
    foundry->available = loading_capacity;
    pthread_cond_init(&foundry->oneIronOneCoal,NULL);
    pthread_cond_init(&foundry->storageAvail,NULL);
    pthread_mutex_init(&foundry->condMutex_foundry,NULL);
    pthread_mutex_init(&foundry->condMutex_foundryStroage,NULL);
    sem_init(&foundry->foundryMutex,0,1);
    foundry->active = 1;

}

void destroy_foundry(Foundry* foundry){
    pthread_cond_destroy(&foundry->oneIronOneCoal);
    pthread_cond_destroy(&foundry->storageAvail);
    pthread_mutex_destroy(&foundry->condMutex_foundry);
    pthread_mutex_destroy(&foundry->condMutex_foundryStroage);
    sem_destroy(&foundry->foundryMutex);
}
void transporter_miner(unsigned int ID,unsigned int interval,Miner* miner,OreType*carriedOre){
    MinerInfo mInfo;
    TransporterInfo tInfo;
    FillMinerInfo(&mInfo,miner->ID,0,0,0);

    FillTransporterInfo(&tInfo,ID,NULL);
    WriteOutput(&mInfo,&tInfo,NULL,NULL,TRANSPORTER_TRAVEL);
    usleep(interval - (interval*0.01) + (rand()%(int)(interval*0.02)));

    sem_wait(&miner->minerMutex);
    (miner->current_count)--;
    sem_post(&miner->minerMutex);

    *carriedOre = miner->oreType;

    sem_wait(&miner->minerMutex);
    FillMinerInfo(&mInfo,miner->ID,miner->oreType,miner->capacity,miner->current_count);
    sem_post(&miner->minerMutex);

    FillTransporterInfo(&tInfo,ID,carriedOre);
    WriteOutput(&mInfo,&tInfo,NULL,NULL,TRANSPORTER_TAKE_ORE);
    usleep(interval - (interval*0.01) + (rand()%(int)(interval*0.02)));

    sem_post(&miner->minerSem);




}
void transporter_smelter(unsigned int ID,unsigned int interval,Smelter* smelter,OreType*carriedOre){
    SmelterInfo sInfo;
    TransporterInfo tInfo;
    FillSmelterInfo(&sInfo,smelter->ID,smelter->oreType,0,0,0);
    FillTransporterInfo(&tInfo,ID,carriedOre);
    WriteOutput(NULL,&tInfo,&sInfo,NULL,TRANSPORTER_TRAVEL);
    usleep(interval - (interval*0.01) + (rand()%(int)(interval*0.02)));

    sem_wait(&smelter->smelterMutex);
    smelter->waiting_ore_count +=1;
    FillSmelterInfo(&sInfo,smelter->ID,smelter->oreType,smelter->loading_capacity,
                    smelter->waiting_ore_count,smelter->total_produce);
    sem_post(&smelter->smelterMutex);

    FillTransporterInfo(&tInfo,ID,carriedOre);
    WriteOutput(NULL,&tInfo,&sInfo,NULL,TRANSPORTER_DROP_ORE);
    usleep(interval - (interval*0.01) + (rand()%(int)(interval*0.02)));

    sem_wait(&smelter->smelterMutex);
    if(smelter->waiting_ore_count >=2){
        pthread_mutex_lock(&smelter->condMutex_smelter);
        pthread_cond_signal(&smelter->twoOres);
        pthread_mutex_unlock(&smelter->condMutex_smelter);
    }
    sem_post(&smelter->smelterMutex);



}

void transporter_foundry(unsigned int ID,unsigned int interval,Foundry* foundry,OreType*carriedOre){
    FoundryInfo fInfo;
    TransporterInfo tInfo;
    FillFoundryInfo(&fInfo,foundry->ID,0,0,0,0);
    FillTransporterInfo(&tInfo,ID,carriedOre);
    WriteOutput(NULL,&tInfo,NULL,&fInfo,TRANSPORTER_TRAVEL);
    usleep(interval - (interval*0.01) + (rand()%(int)(interval*0.02)));
    if(*carriedOre == IRON){
        sem_wait(&foundry->foundryMutex);
        foundry->waiting_iron +=1;
    }else{
        sem_wait(&foundry->foundryMutex);
        foundry->waiting_coal +=1;

    }
    FillFoundryInfo(&fInfo,foundry->ID,foundry->loading_capacity,
                    foundry->waiting_iron,
                    foundry->waiting_coal,foundry->total_produce);
    sem_post(&foundry->foundryMutex);
    FillTransporterInfo(&tInfo,ID,carriedOre);
    WriteOutput(NULL,&tInfo,NULL,&fInfo,TRANSPORTER_DROP_ORE);
    usleep(interval - (interval*0.01) + (rand()%(int)(interval*0.02)));

    sem_wait(&foundry->foundryMutex);
    if(foundry->waiting_iron && foundry->waiting_coal){
        pthread_mutex_lock(&foundry->condMutex_foundry);
        pthread_cond_signal(&foundry->oneIronOneCoal);
        pthread_mutex_unlock(&foundry->condMutex_foundry);

    }
    sem_post(&foundry->foundryMutex);

}

Smelter* find1MissingSmelter(Smelter* smelters,int lenSmelters){
    if(!lenSmelters)
        return NULL;
    for(int i =0; i < lenSmelters;i++){
        pthread_mutex_lock(&smelters[i].condMutex_smelterStorage);
        sem_wait(&smelters[i].smelterMutex);
        if(smelters[i].active && (smelters[i].waiting_ore_count && smelters->available)){
            sem_post(&smelters[i].smelterMutex);
            pthread_mutex_unlock(&smelters[i].condMutex_smelterStorage);
            return &smelters[i];
        }
        sem_post(&smelters[i].smelterMutex);
        pthread_mutex_unlock(&smelters[i].condMutex_smelterStorage);
    }
    return NULL;
}
Foundry* find1MissingFoundries(Foundry* foundries,int lenFoundries,OreType oreType){
    if(!lenFoundries)
        return NULL;
    for(int i =0; i < lenFoundries;i++){
        pthread_mutex_lock(&foundries[i].condMutex_foundryStroage);
        sem_wait(&foundries[i].foundryMutex);
        if(foundries[i].active && (((foundries[i].waiting_coal  && oreType==IRON) ||
                (foundries[i].waiting_iron && oreType == COAL)) && foundries[i].available)){
            sem_post(&foundries[i].foundryMutex);
            pthread_mutex_unlock(&foundries[i].condMutex_foundryStroage);
            return &foundries[i];
        }
        sem_post(&foundries[i].foundryMutex);
        pthread_mutex_unlock(&foundries[i].condMutex_foundryStroage);
    }
    return NULL;

}
Smelter* findAvailableSmelters(Smelter* smelters,int lenSmelters){
    if(!lenSmelters)
        return NULL;
    for(int i =0; i < lenSmelters;i++){
        pthread_mutex_lock(&smelters[i].condMutex_smelterStorage);
        if(smelters->active && smelters->available){
            pthread_mutex_unlock(&smelters[i].condMutex_smelterStorage);
            return &smelters[i];
        }
        pthread_mutex_unlock(&smelters[i].condMutex_smelterStorage);
    }
    return NULL;

}
Foundry* findAvailableFoundries(Foundry* foundries,int lenFoundries){
    if(!lenFoundries)
        return NULL;
    for(int i =0; i < lenFoundries;i++){
        pthread_mutex_lock(&foundries[i].condMutex_foundryStroage);
        if(foundries->active && foundries->available){
            pthread_mutex_unlock(&foundries[i].condMutex_foundryStroage);
            return &foundries[i];
        }
        pthread_mutex_unlock(&foundries[i].condMutex_foundryStroage);
    }
    return NULL;

}
void waitProducer(Smelter* smelters,Foundry* foundries,
        int order,Smelter** resSmelter,
        Foundry** resFoundry,
        int lenFoundries,int lenSmelters,OreType oreType){
    Smelter* smelter;
    Foundry* foundry;
    if(order){

        if((smelter = find1MissingSmelter(smelters,lenSmelters))){
            *resSmelter = smelter;
            return;

        }
        if((foundry = find1MissingFoundries(foundries,lenFoundries,oreType))){
            *resFoundry = foundry;
            return;;
        }
        if((smelter = findAvailableSmelters(smelters,lenSmelters))){
            *resSmelter = smelter;
            return;
        }
        if((foundry = findAvailableFoundries(foundries,lenFoundries))){
            *resFoundry = foundry;
            return;;
        }

    }else{
        if((foundry = find1MissingFoundries(foundries,lenFoundries,oreType))){
            *resFoundry = foundry;
            return;;
        }
        if((smelter = find1MissingSmelter(smelters,lenSmelters))){
            *resSmelter = smelter;
            return;

        }
        if((foundry = findAvailableFoundries(foundries,lenFoundries))){
            *resFoundry = foundry;
            return;
        }
        if((smelter = findAvailableSmelters(smelters,lenSmelters))){
            *resSmelter = smelter;
            return;
        }

    }

}

Miner* waitNextLoad(Transporter* transporter,int numMiner,int lastID){
    int avail = 0;
    int idx= 0;
    int next = lastID;
    while(1){
        pthread_mutex_lock(&transporter->miners[next].minerActiveMutex);
        if(transporter->miners[next].reserved_count){
            pthread_mutex_unlock(&transporter->miners[next].minerActiveMutex);
            avail = 1;
            idx = next;
            break;
        }
        if( ((next+1) % numMiner) == lastID){
            pthread_mutex_unlock(&transporter->miners[next].minerActiveMutex);
            break;
        }
        pthread_mutex_unlock(&transporter->miners[next].minerActiveMutex);
        next = (next+1) % numMiner;
    }
    if(avail){
        return &transporter->miners[idx];
    }
    return &transporter->miners[lastID];
}

void* transporter_subroutine(void* transporterArg){
    Transporter* transporter = (Transporter*)transporterArg;
    TransporterInfo tInfo;
    FillTransporterInfo(&tInfo,transporter->ID,NULL);
    WriteOutput(NULL,&tInfo,NULL,NULL,TRANSPORTER_CREATED);
    int lastID = 0;
    while(1){
        OreType carriedOre = -1;
        Miner * miner = waitNextLoad(transporter,2,lastID);
        pthread_mutex_lock(&miner->minerActiveMutex);
        if(miner->reserved_count){
            miner->reserved_count --;
            pthread_mutex_unlock(&miner->minerActiveMutex);
        }else if(miner->active){
            pthread_cond_wait(&miner->minerProduced,&miner->minerActiveMutex);
            miner->reserved_count--;
            pthread_mutex_unlock(&miner->minerActiveMutex);
        }else{

            pthread_mutex_unlock(&miner->minerActiveMutex);
            break;
        }

        transporter_miner(transporter->ID,transporter->interval,miner,&carriedOre);
        /*
        Smelter * smelter = transporter->smelters;
        pthread_mutex_lock(&smelter->condMutex_smelterStorage);

        while(smelter->available ==0)
            pthread_cond_wait(&smelter->storageAvail,&smelter->condMutex_smelterStorage);
        smelter->available -= 1;
        pthread_mutex_unlock(&smelter->condMutex_smelterStorage);
        transporter_smelter(transporter->ID,transporter->interval,smelter,&carriedOre);

        //usleep(args.interval - (args.interval*0.01) + (rand()%(int)(args.interval*0.02)));
        */

        /*
        Foundry *foundry = transporter->foundries;
        pthread_mutex_lock(&foundry->condMutex_foundryStroage);
        while(foundry->available == 0)
            pthread_cond_wait(&foundry->storageAvail,&foundry->condMutex_foundryStroage);
        foundry->available -=1;
        pthread_mutex_unlock(&foundry->condMutex_foundryStroage);
        transporter_foundry(transporter->ID,transporter->interval,foundry,&carriedOre);
        lastID= (lastID+1) % 2;
         */
        Foundry* foundry = NULL;
        Smelter* smelter = NULL;
        waitProducer(transporter->smelters,transporter->foundries,lastID%2,&smelter,&foundry,1,1,carriedOre);
        if(foundry){
            pthread_mutex_lock(&foundry->condMutex_foundryStroage);
            foundry->available --;
            pthread_mutex_unlock(&foundry->condMutex_foundryStroage);
            transporter_foundry(transporter->ID,transporter->interval,foundry,&carriedOre);

        }else if(smelter){
            pthread_mutex_lock(&smelter->condMutex_smelterStorage);
            smelter->available--;
            pthread_mutex_unlock(&smelter->condMutex_smelterStorage);
            transporter_smelter(transporter->ID,transporter->interval,smelter,&carriedOre);

        }else{

            pthread_mutex_lock(&producerMutex);
            pthread_cond_wait(&producerFinished,&producerMutex);
            pthread_mutex_unlock(&producerMutex);

            waitProducer(transporter->smelters,transporter->foundries,lastID%2,&smelter,&foundry,1,0,carriedOre);
            if(foundry){
                pthread_mutex_lock(&foundry->condMutex_foundryStroage);
                foundry->available --;
                pthread_mutex_unlock(&foundry->condMutex_foundryStroage);
                transporter_foundry(transporter->ID,transporter->interval,foundry,&carriedOre);

            }else if(smelter){
                pthread_mutex_lock(&smelter->condMutex_smelterStorage);
                smelter->available--;
                pthread_mutex_unlock(&smelter->condMutex_smelterStorage);
                transporter_smelter(transporter->ID,transporter->interval,smelter,&carriedOre);

            }else{
                pthread_mutex_lock(&producerMutex);
                pthread_cond_signal(&producerFinished);
                pthread_mutex_unlock(&producerMutex);
                continue;
            }


        }
        lastID= (lastID+1) % 2;

    }

}

void* miner_subroutine(void* minerArg){
    Miner* miner = (Miner *)minerArg;
    MinerInfo mInfo;
    sem_wait(&miner->minerMutex);
    FillMinerInfo(&mInfo,miner->ID,miner->oreType,miner->capacity,miner->current_count);
    sem_post(&miner->minerMutex);
    WriteOutput(&mInfo,NULL,NULL,NULL,MINER_CREATED);

    while(miner->totalOre > 0){
        sem_wait(&miner->minerSem);

        sem_wait(&miner->minerMutex);
        FillMinerInfo(&mInfo,miner->ID,miner->oreType,miner->capacity,miner->current_count);
        sem_post(&miner->minerMutex);

        WriteOutput(&mInfo,NULL,NULL,NULL,MINER_STARTED);
        usleep(miner->interval - (miner->interval*0.01) + (rand()%(int)(miner->interval*0.02)));

        sem_wait(&miner->minerMutex);
        miner->current_count++;
        FillMinerInfo(&mInfo,miner->ID,miner->oreType,miner->capacity,miner->current_count);
        sem_post(&miner->minerMutex);

        pthread_mutex_lock(&miner->minerActiveMutex);
        miner->reserved_count++;
        pthread_cond_signal(&miner->minerProduced);
        pthread_mutex_unlock(&miner->minerActiveMutex);

        WriteOutput(&mInfo,NULL,NULL,NULL,MINER_FINISHED);
        usleep(miner->interval - (miner->interval*0.01) + (rand()%(int)(miner->interval*0.02)));

        miner->totalOre--;

    }
    // Miner stopped semaphore here
    pthread_mutex_lock(&miner->minerActiveMutex);
    miner->active =0;
    pthread_mutex_unlock(&miner->minerActiveMutex);

    sem_wait(&miner->minerMutex);
    FillMinerInfo(&mInfo,miner->ID,miner->oreType,miner->capacity,miner->current_count);
    sem_post(&miner->minerMutex);

    WriteOutput(&mInfo,NULL,NULL,NULL,MINER_STOPPED);

}

void* smelter_subroutine(void* smelterArg){
    SmelterInfo sInfo;
    Smelter* smelter = (Smelter*)smelterArg;

    sem_wait(&smelter->smelterMutex);
    FillSmelterInfo(&sInfo,smelter->ID,smelter->oreType,
                    smelter->loading_capacity,
                    smelter->waiting_ore_count,smelter->total_produce);
    sem_post(&smelter->smelterMutex);

    WriteOutput(NULL,NULL,&sInfo,NULL,SMELTER_CREATED);
    while(1){
        sem_wait(&smelter->smelterMutex);
        if(smelter->waiting_ore_count < 2){
            sem_post(&smelter->smelterMutex);
            gettimeofday(&smelter->tv_smelter, NULL);
            smelter->ts_smelter.tv_sec = time(NULL) + 5;
            smelter->ts_smelter.tv_sec += smelter->ts_smelter.tv_nsec / (1000 * 1000 * 1000);
            smelter->ts_smelter.tv_nsec %= (1000 * 1000 * 1000);
            pthread_mutex_lock(&smelter->condMutex_smelter);
            pthread_cond_timedwait(&smelter->twoOres,&smelter->condMutex_smelter,&smelter->ts_smelter);
            //pthread_cond_wait(&smelter->twoOres,&smelter->condMutex_smelter);
            pthread_mutex_unlock(&smelter->condMutex_smelter);
            sem_wait(&smelter->smelterMutex);
            if(smelter->waiting_ore_count < 2){
                sem_post(&smelter->smelterMutex);
                break;
            }
        }


        FillSmelterInfo(&sInfo,smelter->ID,smelter->oreType,
                        smelter->loading_capacity,smelter->waiting_ore_count,smelter->total_produce);
        sem_post(&smelter->smelterMutex);

        WriteOutput(NULL,NULL,&sInfo,NULL,SMELTER_STARTED);
        usleep(smelter->interval - (smelter->interval*0.01) + (rand()%(int)(smelter->interval*0.02)));

        // Smelter produced
        sem_wait(&smelter->smelterMutex);
        smelter->total_produce += 1;
        smelter->waiting_ore_count -= 2;
        FillSmelterInfo(&sInfo,smelter->ID,smelter->oreType,
                        smelter->loading_capacity,smelter->waiting_ore_count,smelter->total_produce);
        sem_post(&smelter->smelterMutex);

        pthread_mutex_lock(&smelter->condMutex_smelterStorage);
        smelter->available += 2;
        pthread_cond_signal(&smelter->storageAvail);
        pthread_mutex_unlock(&smelter->condMutex_smelterStorage);


        WriteOutput(NULL,NULL,&sInfo,NULL,SMELTER_FINISHED);
    }
    //Smelter stopped
    sem_wait(&smelter->smelterMutex);
    FillSmelterInfo(&sInfo,smelter->ID,smelter->oreType,
                    smelter->loading_capacity,smelter->waiting_ore_count,smelter->total_produce);
    sem_post(&smelter->smelterMutex);
    WriteOutput(NULL,NULL,&sInfo,NULL,SMELTER_STOPPED);

}

void* foundry_subroutine(void* foundryArg){
    FoundryInfo fInfo;
    Foundry* foundry = (Foundry*)foundryArg;

    sem_wait(&foundry->foundryMutex);
    FillFoundryInfo(&fInfo,foundry->ID,foundry->loading_capacity,
                    foundry->waiting_iron,foundry->waiting_coal,
                    foundry->total_produce);
    sem_post(&foundry->foundryMutex);
    WriteOutput(NULL,NULL,NULL,&fInfo,FOUNDRY_CREATED);
    while(1){


        sem_wait(&foundry->foundryMutex);
        if(!foundry->waiting_iron || !foundry->waiting_coal){
            sem_post(&foundry->foundryMutex);
            gettimeofday(&foundry->tv_foundry, NULL);
            foundry->ts_foundry.tv_sec = time(NULL) + 5;
            foundry->ts_foundry.tv_sec += foundry->ts_foundry.tv_nsec / (1000 * 1000 * 1000);
            foundry->ts_foundry.tv_nsec %= (1000 * 1000 * 1000);
            pthread_mutex_lock(&foundry->condMutex_foundry);
            pthread_cond_timedwait(&foundry->oneIronOneCoal,&foundry->condMutex_foundry,&foundry->ts_foundry);
            pthread_mutex_unlock(&foundry->condMutex_foundry);
            sem_wait(&foundry->foundryMutex);
            if(!foundry->waiting_iron || !foundry->waiting_coal){
                sem_post(&foundry->foundryMutex);
                break;
            }
        }

        FillFoundryInfo(&fInfo,foundry->ID,foundry->loading_capacity,
                        foundry->waiting_iron,foundry->waiting_coal,
                        foundry->total_produce);
        sem_post(&foundry->foundryMutex);
        WriteOutput(NULL,NULL,NULL,&fInfo,FOUNDRY_STARTED);
        usleep(foundry->interval - (foundry->interval*0.01) + (rand()%(int)(foundry->interval*0.02)));

        //Foundry produced
        sem_wait(&foundry->foundryMutex);
        foundry->total_produce+=1;
        foundry->waiting_coal-=1;
        foundry->waiting_iron-=1;
        FillFoundryInfo(&fInfo,foundry->ID,foundry->loading_capacity,
                        foundry->waiting_iron,foundry->waiting_coal,
                        foundry->total_produce);
        sem_post(&foundry->foundryMutex);

        //TODO: Storage code here...
        pthread_mutex_lock(&foundry->condMutex_foundryStroage);
        foundry->available +=2;
        pthread_mutex_unlock(&foundry->condMutex_foundryStroage);

        pthread_mutex_lock(&producerMutex);
        pthread_cond_signal(&producerFinished);
        pthread_mutex_unlock(&producerMutex);

        WriteOutput(NULL,NULL,NULL,&fInfo,FOUNDRY_FINISHED);
    }
    // FoundryStopped()
    pthread_mutex_lock(&foundry->condMutex_foundryStroage);
    foundry->active =0;
    pthread_mutex_unlock(&foundry->condMutex_foundryStroage);

    pthread_mutex_lock(&producerMutex);
    pthread_cond_signal(&producerFinished);
    pthread_mutex_unlock(&producerMutex);


    sem_wait(&foundry->foundryMutex);
    FillFoundryInfo(&fInfo,foundry->ID,foundry->loading_capacity,
                    foundry->waiting_iron,foundry->waiting_coal,
                    foundry->total_produce);
    sem_post(&foundry->foundryMutex);
    WriteOutput(NULL,NULL,NULL,&fInfo,FOUNDRY_STOPPED);


}

int main() {
    time_t t;
    srand((unsigned) time(&t));
    InitWriteOutput();
    pthread_t minerID1,minerID2,transporterID,transporter2,smelterID,foundryID;
    Miner * miner = malloc(sizeof(Miner)*2);
    Transporter *transporter = malloc(sizeof(Transporter)*2);
    Smelter* smelter = malloc(sizeof(Smelter));
    Foundry* foundry = malloc(sizeof(Foundry));
    init_miner(&miner[0],1,IRON,10,50,50);
    init_miner(&miner[1],2,COAL,10,50,40);
    init_smelter(smelter,1,IRON,4,70);
    init_foundry(foundry,1,6,50);
    init_transporter(&transporter[0],1,50,miner,smelter,foundry);
    init_transporter(&transporter[1],2,50,miner,smelter,foundry);


    pthread_create(&minerID1, NULL, miner_subroutine,(void*)&miner[0]);
    pthread_create(&minerID2, NULL, miner_subroutine,(void*)&miner[1]);
    pthread_create(&smelterID, NULL, smelter_subroutine, (void*)smelter);
    pthread_create(&transporterID, NULL, transporter_subroutine, (void*)&transporter[0]);
    pthread_create(&transporter2, NULL, transporter_subroutine, (void*)&transporter[1]);
    pthread_create(&foundryID, NULL, foundry_subroutine, (void*)foundry);

    pthread_join(minerID1,NULL);
    pthread_join(minerID2,NULL);
    pthread_join(transporterID,NULL);
    pthread_join(transporter2,NULL);
    pthread_join(foundryID,NULL);
    pthread_join(smelterID,NULL);
    destroy_miner(&miner[0]);
    destroy_miner(&miner[1]);
    destroy_transporter(&transporter[0]);
    destroy_transporter(&transporter[1]);
    destroy_foundry(foundry);
    destroy_smelter(smelter);
    free(miner);
    free(transporter);
    free(foundry);
    free(smelter);
    //pthread_join(foundryID,NULL);


    return 0;
}
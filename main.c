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
    pthread_t  minerThread;
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
    pthread_t  smelterThread;
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
    unsigned int available_iron;
    unsigned int available_coal;
    sem_t foundryMutex;
    pthread_t  foundryThread;
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
    unsigned int numMiners;
    unsigned int numSmelters;
    unsigned int numFoundries;
    pthread_t transporterThread;
    Miner* miners;
    Smelter* smelters;
    Foundry* foundries;


}Transporter;

pthread_cond_t producerFinished = PTHREAD_COND_INITIALIZER;
pthread_mutex_t producerMutex = PTHREAD_MUTEX_INITIALIZER;


int numAvailableMiners = 0;
int nonActiveMiners = 0;
pthread_mutex_t minerAvailableMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t availableMinerCond = PTHREAD_COND_INITIALIZER;


int numNonActiveProducers = 0;
int numReadyProducers[3] = {0,0,0};
pthread_mutex_t producerReadyMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_producerReadyMutex = PTHREAD_COND_INITIALIZER;
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
    foundry->available_iron = foundry->available_coal = loading_capacity;
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

Smelter* find1MissingSmelter(Smelter* smelters,int lenSmelters,OreType oreType){
    if(!lenSmelters)
        return NULL;
    if(oreType == COAL)
        return  NULL;
    for(int i =0; i < lenSmelters;i++){
        if(smelters[i].oreType != oreType)
            continue;
        pthread_mutex_lock(&smelters[i].condMutex_smelterStorage);
        sem_wait(&smelters[i].smelterMutex);
        if(smelters[i].active && (smelters[i].waiting_ore_count && smelters[i].available)){
            sem_post(&smelters[i].smelterMutex);
            smelters[i].available--;
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
    if(oreType == COPPER)
        return NULL;
    for(int i =0; i < lenFoundries;i++){
        pthread_mutex_lock(&foundries[i].condMutex_foundryStroage);
        sem_wait(&foundries[i].foundryMutex);

        if(foundries[i].active){
            if((foundries[i].waiting_coal && oreType == IRON) && foundries[i].available_iron){
                foundries[i].available_iron--;
                sem_post(&foundries[i].foundryMutex);
                pthread_mutex_unlock(&foundries[i].condMutex_foundryStroage);
                return &foundries[i];
            }else if((foundries[i].waiting_iron && oreType == COAL) && foundries[i].available_coal){
                foundries[i].available_coal--;
                sem_post(&foundries[i].foundryMutex);
                pthread_mutex_unlock(&foundries[i].condMutex_foundryStroage);
                return &foundries[i];
            }else{
                sem_post(&foundries[i].foundryMutex);
                pthread_mutex_unlock(&foundries[i].condMutex_foundryStroage);
            }
        }else{
            sem_post(&foundries[i].foundryMutex);
            pthread_mutex_unlock(&foundries[i].condMutex_foundryStroage);
        }

    }
    return NULL;

}
Smelter* findAvailableSmelters(Smelter* smelters,int lenSmelters,OreType oreType){
    if(!lenSmelters)
        return NULL;
    if(oreType == COAL)
        return NULL;
    for(int i =0; i < lenSmelters;i++){
        if(smelters[i].oreType != oreType)
            continue;
        pthread_mutex_lock(&smelters[i].condMutex_smelterStorage);
        if(smelters[i].active && smelters[i].available){
            smelters[i].available--;
            pthread_mutex_unlock(&smelters[i].condMutex_smelterStorage);
            return &smelters[i];
        }
        pthread_mutex_unlock(&smelters[i].condMutex_smelterStorage);
    }
    return NULL;

}
Foundry* findAvailableFoundries(Foundry* foundries,int lenFoundries,OreType oreType){
    if(!lenFoundries)
        return NULL;
    if(oreType == COPPER)
        return NULL;
    for(int i =0; i < lenFoundries;i++){
        pthread_mutex_lock(&foundries[i].condMutex_foundryStroage);
        if(foundries[i].active){
            if((oreType ==IRON && foundries[i].available_iron) &&
                (foundries[i].available_coal != foundries[i].loading_capacity
                || foundries[i].available_iron == foundries[i].loading_capacity)){
                foundries[i].available_iron--;
                pthread_mutex_unlock(&foundries[i].condMutex_foundryStroage);
                return &foundries[i];
            }else if((oreType ==COAL && foundries[i].available_coal)  &&
                    (foundries[i].available_iron != foundries[i].loading_capacity
                     || foundries[i].available_coal == foundries[i].loading_capacity)){
                foundries[i].available_coal--;
                pthread_mutex_unlock(&foundries[i].condMutex_foundryStroage);
                return &foundries[i];
            }else{
                pthread_mutex_unlock(&foundries[i].condMutex_foundryStroage);
            }
        }else{
            pthread_mutex_unlock(&foundries[i].condMutex_foundryStroage);
        }

    }
    return NULL;

}
void waitProducer(Smelter* smelters,Foundry* foundries,
                  int order,Smelter** resSmelter,
                  Foundry** resFoundry,
                  int lenFoundries,int lenSmelters,OreType oreType,int transporterID){
    Smelter* smelter;
    Foundry* foundry;
    if(order){

        if((smelter = find1MissingSmelter(smelters,lenSmelters,oreType))){
            *resSmelter = smelter;
            return;

        }
        if((foundry = find1MissingFoundries(foundries,lenFoundries,oreType))){
            *resFoundry = foundry;

            return;
        }
        if((smelter = findAvailableSmelters(smelters,lenSmelters,oreType))){
            *resSmelter = smelter;
            return;
        }
        if((foundry = findAvailableFoundries(foundries,lenFoundries,oreType))){
            *resFoundry = foundry;
            return;
        }

    }else{

        if((foundry = find1MissingFoundries(foundries,lenFoundries,oreType))){
            *resFoundry = foundry;
            return;
        }
        if((smelter = find1MissingSmelter(smelters,lenSmelters,oreType))){
            *resSmelter = smelter;
            return;

        }
        if((foundry = findAvailableFoundries(foundries,lenFoundries,oreType))){
            *resFoundry = foundry;
            return;
        }
        if((smelter = findAvailableSmelters(smelters,lenSmelters,oreType))){
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
    OreType carriedOre = -1;
    Miner* miner;
    while(1) {
        pthread_mutex_lock(&minerAvailableMutex);
        if (!numAvailableMiners && nonActiveMiners == transporter->numMiners) {
            pthread_cond_signal(&availableMinerCond);
            pthread_mutex_unlock(&minerAvailableMutex);
            FillTransporterInfo(&tInfo,transporter->ID,&carriedOre);
            WriteOutput(NULL,&tInfo,NULL,NULL,TRANSPORTER_STOPPED);
            return NULL;

        } else if (numAvailableMiners) {
            numAvailableMiners--;
            pthread_mutex_unlock(&minerAvailableMutex);
            miner = waitNextLoad(transporter, transporter->numMiners, lastID);

            pthread_mutex_lock(&miner->minerActiveMutex);
            miner->reserved_count--;
            pthread_mutex_unlock(&miner->minerActiveMutex);

        } else {
            pthread_cond_wait(&availableMinerCond, &minerAvailableMutex);
            pthread_mutex_unlock(&minerAvailableMutex);
            continue;
        }
        transporter_miner(transporter->ID,transporter->interval,miner,&carriedOre);

        pthread_mutex_lock(&producerReadyMutex);
        // Round
        round:
        if(!numReadyProducers[carriedOre] && numNonActiveProducers == (transporter->numFoundries+ transporter->numSmelters)) {
            pthread_cond_signal(&cond_producerReadyMutex);
            pthread_mutex_unlock(&producerReadyMutex);
            continue;
        }else if(numReadyProducers[carriedOre]){
            numReadyProducers[carriedOre]--;
            pthread_mutex_unlock(&producerReadyMutex);
            Foundry* foundry = NULL;
            Smelter* smelter = NULL;
            waitProducer(transporter->smelters,transporter->foundries,lastID%2,
                         &smelter,&foundry,transporter->numSmelters,transporter->numSmelters,carriedOre,transporter->ID);

            if(foundry){
                transporter_foundry(transporter->ID,transporter->interval,foundry,&carriedOre);

            }else if(smelter){
                transporter_smelter(transporter->ID,transporter->interval,smelter,&carriedOre);

            }
        }else{
            pthread_cond_wait(&cond_producerReadyMutex,&producerReadyMutex);
            goto round;
        }
        lastID= (lastID+1) % (transporter->numMiners);

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

        pthread_mutex_lock(&minerAvailableMutex);
        numAvailableMiners++;
        pthread_cond_signal(&availableMinerCond);
        pthread_mutex_unlock(&minerAvailableMutex);


        WriteOutput(&mInfo,NULL,NULL,NULL,MINER_FINISHED);
        usleep(miner->interval - (miner->interval*0.01) + (rand()%(int)(miner->interval*0.02)));

        miner->totalOre--;

    }
    // Miner stopped semaphore here
    pthread_mutex_lock(&miner->minerActiveMutex);
    miner->active =0;
    pthread_mutex_unlock(&miner->minerActiveMutex);

    pthread_mutex_lock(&minerAvailableMutex);
    nonActiveMiners++;
    pthread_cond_signal(&availableMinerCond);
    pthread_mutex_unlock(&minerAvailableMutex);

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

        smelter->waiting_ore_count -= 2;
        FillSmelterInfo(&sInfo,smelter->ID,smelter->oreType,
                        smelter->loading_capacity,smelter->waiting_ore_count,smelter->total_produce);
        sem_post(&smelter->smelterMutex);

        WriteOutput(NULL,NULL,&sInfo,NULL,SMELTER_STARTED);
        usleep(smelter->interval - (smelter->interval*0.01) + (rand()%(int)(smelter->interval*0.02)));

        // Smelter produced
        sem_wait(&smelter->smelterMutex);
        smelter->total_produce += 1;
        FillSmelterInfo(&sInfo,smelter->ID,smelter->oreType,
                        smelter->loading_capacity,smelter->waiting_ore_count,smelter->total_produce);
        sem_post(&smelter->smelterMutex);

        pthread_mutex_lock(&smelter->condMutex_smelterStorage);
        smelter->available += 2;
        pthread_cond_signal(&smelter->storageAvail);
        pthread_mutex_unlock(&smelter->condMutex_smelterStorage);

        pthread_mutex_lock(&producerReadyMutex);
        numReadyProducers[smelter->oreType]+=2;
        pthread_cond_signal(&cond_producerReadyMutex);
        pthread_mutex_unlock(&producerReadyMutex);


        WriteOutput(NULL,NULL,&sInfo,NULL,SMELTER_FINISHED);
    }
    //Smelter stopped
    pthread_mutex_lock(&smelter->condMutex_smelterStorage);
    smelter->active =0;
    pthread_mutex_unlock(&smelter->condMutex_smelterStorage);

    pthread_mutex_lock(&producerReadyMutex);
    numNonActiveProducers++;
    pthread_cond_signal(&cond_producerReadyMutex);
    pthread_mutex_unlock(&producerReadyMutex);

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
        foundry->waiting_coal-=1;
        foundry->waiting_iron-=1;
        FillFoundryInfo(&fInfo,foundry->ID,foundry->loading_capacity,
                        foundry->waiting_iron,foundry->waiting_coal,
                        foundry->total_produce);
        sem_post(&foundry->foundryMutex);


        WriteOutput(NULL,NULL,NULL,&fInfo,FOUNDRY_STARTED);
        usleep(foundry->interval - (foundry->interval*0.01) + (rand()%(int)(foundry->interval*0.02)));

        //Foundry produced
        sem_wait(&foundry->foundryMutex);
        foundry->total_produce+=1;
        FillFoundryInfo(&fInfo,foundry->ID,foundry->loading_capacity,
                        foundry->waiting_iron,foundry->waiting_coal,
                        foundry->total_produce);
        sem_post(&foundry->foundryMutex);

        pthread_mutex_lock(&foundry->condMutex_foundryStroage);
        foundry->available_iron ++;
        foundry->available_coal ++;
        pthread_mutex_unlock(&foundry->condMutex_foundryStroage);

        pthread_mutex_lock(&producerMutex);
        pthread_cond_signal(&producerFinished);
        pthread_mutex_unlock(&producerMutex);

        pthread_mutex_lock(&producerReadyMutex);
        numReadyProducers[COAL]++;
        numReadyProducers[IRON]++;
        pthread_cond_signal(&cond_producerReadyMutex);
        pthread_mutex_unlock(&producerReadyMutex);

        WriteOutput(NULL,NULL,NULL,&fInfo,FOUNDRY_FINISHED);
    }
    // FoundryStopped()
    pthread_mutex_lock(&foundry->condMutex_foundryStroage);
    foundry->active =0;
    pthread_mutex_unlock(&foundry->condMutex_foundryStroage);


    pthread_mutex_lock(&producerReadyMutex);
    numNonActiveProducers++;
    pthread_cond_signal(&cond_producerReadyMutex);

    pthread_mutex_unlock(&producerReadyMutex);

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


void readInput(Transporter** transporters,Miner** miners,Smelter** smelters,Foundry** foundries,
               unsigned int* numMiners,
               unsigned int* numTransporters,
               unsigned int* numSmelters,unsigned int* numFoundries){
    unsigned int minerInterval,minerCapacity,
            minerTotalOre,transporterInterval,
            smelterInterval,foundryInterval,smelterCapacity,foundryCapacity;
    OreType minerOreType,smelterOreType;
    fscanf(stdin,"%d",numMiners);
    *miners = malloc(sizeof(Miner)*(*numMiners));
    for(int i=0; i< *numMiners;i++){
        fscanf(stdin,"%d %d %d %d",&minerInterval,&minerCapacity,&minerOreType,&minerTotalOre);
        init_miner(&(*miners)[i],(i+1),minerOreType,minerCapacity,minerInterval,minerTotalOre);
    }
    fscanf(stdin,"%d",numTransporters);
    *transporters = malloc(sizeof(Transporter)*(*numTransporters));
    for(int i=0; i< *numTransporters;i++){
        fscanf(stdin,"%d",&transporterInterval);
        init_transporter(&(*transporters)[i],i+1,transporterInterval,*miners,NULL,NULL);
    }
    fscanf(stdin,"%d",numSmelters);
    *smelters = malloc(sizeof(Smelter)*(*numSmelters));
    for(int i=0; i< *numSmelters;i++){
        fscanf(stdin,"%d %d %d",&smelterInterval,&smelterCapacity,&smelterOreType);
        init_smelter(&(*smelters)[i],i+1,smelterOreType,smelterCapacity,smelterInterval);
        numReadyProducers[smelterOreType] += smelterCapacity;
    }
    fscanf(stdin,"%d",numFoundries);
    *foundries = malloc(sizeof(Foundry)*(*numFoundries));
    for(int i=0; i< *numFoundries;i++){
        fscanf(stdin,"%d %d",&foundryInterval,&foundryCapacity);
        init_foundry(&(*foundries)[i],i+1,foundryCapacity,foundryInterval);
        numReadyProducers[IRON] += foundryCapacity;
        numReadyProducers[COAL] += foundryCapacity;
    }
    for(int i=0; i< *numTransporters;i++){
        (*transporters)[i].foundries = *foundries;
        (*transporters)[i].smelters =  *smelters;
        (*transporters)[i].numMiners = *numMiners;
        (*transporters)[i].numSmelters = *numSmelters;
        (*transporters)[i].numFoundries = *numFoundries;
    }



}
int main() {
    time_t t;
    srand((unsigned) time(&t));
    InitWriteOutput();
    Transporter* transporters;
    Smelter* smelters;
    Miner* miners;
    Foundry* foundries;
    unsigned  int numTransporters,numSmelters,numMiners,numFoundries;
    readInput(&transporters,&miners,&smelters,&foundries,&numMiners,&numTransporters,&numSmelters,&numFoundries);
    for(int i= 0; i< numMiners;i++){
        pthread_create(&miners[i].minerThread, NULL, miner_subroutine,(void*)&miners[i]);

    }
    for(int i= 0; i< numFoundries;i++){
        pthread_create(&foundries[i].foundryThread, NULL, foundry_subroutine,(void*)&foundries[i]);

    }
    for(int i= 0; i< numSmelters;i++){
        pthread_create(&smelters[i].smelterThread, NULL, smelter_subroutine,(void*)&smelters[i]);

    }
    for(int i= 0; i< numTransporters;i++){
        pthread_create(&transporters[i].transporterThread, NULL, transporter_subroutine,(void*)&transporters[i]);

    }

    // JOIN

    for(int i= 0; i< numMiners;i++){
        pthread_join(miners[i].minerThread,NULL);
        //destroy_miner(&miners[i]);
    }
    for(int i= 0; i< numFoundries;i++){
        pthread_join(foundries[i].foundryThread,NULL);
        //destroy_foundry(&foundries[i]);

    }
    for(int i= 0; i< numSmelters;i++){
        pthread_join(smelters[i].smelterThread,NULL);
        //destroy_smelter(&smelters[i]);

    }
    for(int i= 0; i< numTransporters;i++){
        pthread_join(transporters[i].transporterThread,NULL);
        //destroy_transporter(&transporters[i]);
    }

    // DESTROY

    for(int i= 0; i< numMiners;i++){

        destroy_miner(&miners[i]);
    }
    for(int i= 0; i< numFoundries;i++){

        destroy_foundry(&foundries[i]);

    }
    for(int i= 0; i< numSmelters;i++){

        destroy_smelter(&smelters[i]);

    }
    for(int i= 0; i< numTransporters;i++){

        destroy_transporter(&transporters[i]);
    }
    free(miners);
    free(transporters);
    free(foundries);
    free(smelters);


    return 0;
}
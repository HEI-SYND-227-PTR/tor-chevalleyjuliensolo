#include "stm32f7xx_hal.h"
#include <stdio.h>
#include <string.h>
#include "main.h"

typedef struct bitFAddr_{
	uint8_t sapi:3;		
	uint8_t address:4;
	uint8_t :1;}bitFAddr;

typedef union mapAddr{
			bitFAddr myAddr;
			uint8_t frame;}AddressMap;
		
typedef struct bitFStat_{
	uint8_t ack:1;		
	uint8_t read:1;
	uint8_t crc:6;
			
			}bitFStat;
		
typedef union mapStat{
			bitFStat status; 
			uint8_t frame;
		}StatusMap;

void MacReceiver(void *argument){
	
	osStatus_t nextPhyRElem;
	uint8_t* appPtr;
	uint8_t userDatalength;
	
	struct queueMsg_t fromQueueMacR, toQueueAppR;
	
	for (;;){
		nextPhyRElem = osMessageQueueGet(queue_macR_id, &fromQueueMacR,
			NULL,	osWaitForever);

		if(nextPhyRElem ==osOK){
			char* myCharPtr = fromQueueMacR.anyPtr;
			AddressMap srcAddr;
			srcAddr.frame=(*myCharPtr);
			AddressMap destAddr;
			destAddr.frame=(*(myCharPtr+1));	
			
			//If I recieved a token => TOKEN
			if(myCharPtr[0]==0xFF){
					fromQueueMacR.type = TOKEN;
					osMessageQueuePut(queue_macS_id, &fromQueueMacR, osPriorityNormal, osWaitForever);
				}
			
			//Reception of a message => DATA_IND and TO_PHY 
				userDatalength = myCharPtr[2];
				//If I am destination of message or broadcast => DATA_IND
				if((destAddr.myAddr.address == gTokenInterface.myAddress)|| (destAddr.myAddr.address == 0xF)){
					//Calculate CRC frome recieved frame
						StatusMap myStatus;
						myStatus.frame=myCharPtr[userDatalength+3];
						uint8_t calcCRC=0;
						for(uint8_t i=0;i<(userDatalength+3);i++){
						calcCRC+=myCharPtr[i];}
						calcCRC = calcCRC & 0x3F;
						//If CRC is correct => set Ack bit to 1
						if(myStatus.status.crc == calcCRC){
							myStatus.status.ack=1U;
							myCharPtr[userDatalength+3]=myStatus.frame;
			//IF DATA_IND is TIME - DONE  
						if(destAddr.myAddr.sapi == TIME_SAPI){
							myStatus.status.read=1U;
							myCharPtr[userDatalength+3]=myStatus.frame;
							
										//Send to APP
							appPtr=osMemoryPoolAlloc(memPool, osWaitForever);
							memcpy(appPtr,&myCharPtr[3],userDatalength);
							appPtr[userDatalength]=0;
							toQueueAppR.type = DATA_IND;
							toQueueAppR.addr=srcAddr.myAddr.address;
							toQueueAppR.sapi=srcAddr.myAddr.sapi;
							toQueueAppR.anyPtr=appPtr;
							osMessageQueuePut(queue_timeR_id, &toQueueAppR, osPriorityNormal, osWaitForever);
							
							//Send back to Phy : read=1, ack=1
							if(srcAddr.myAddr.address != gTokenInterface.myAddress){
								fromQueueMacR.type = TO_PHY;
								osMessageQueuePut(queue_phyS_id, &fromQueueMacR, osPriorityNormal, osWaitForever);
							}
						}
			//IF DATA_IND is CHAT - DONE
						else if((destAddr.myAddr.sapi==CHAT_SAPI)&&
							(gTokenInterface.station_list[MYADDRESS] & (1 << CHAT_SAPI))){
							myStatus.status.read=1U;
							myCharPtr[userDatalength+3]=myStatus.frame;
							//Send to APP
							appPtr=osMemoryPoolAlloc(memPool, osWaitForever);
							memcpy(appPtr,&myCharPtr[3],userDatalength);
							appPtr[userDatalength]=0;
							toQueueAppR.type = DATA_IND;
							toQueueAppR.addr=srcAddr.myAddr.address;
							toQueueAppR.sapi= srcAddr.myAddr.sapi;
							toQueueAppR.anyPtr=appPtr;
							osMessageQueuePut(queue_chatR_id, &toQueueAppR, osPriorityNormal, osWaitForever);
							
							//Send to Phy - with read =1 and ack=1
							if(srcAddr.myAddr.address != gTokenInterface.myAddress){
							fromQueueMacR.type = TO_PHY;
							osMessageQueuePut(queue_phyS_id, &fromQueueMacR, osPriorityNormal, osWaitForever);
							}
						}
			//Message with good CRC (ack=1) but no sapi to read it (read=0)
						else{
							osMessageQueuePut(queue_phyS_id, &fromQueueMacR, osPriorityNormal, osWaitForever);
						}
				
					}	
		 //If CRC is not correct (ack=0), read=1
					else{
					fromQueueMacR.type = TO_PHY;
					myStatus.status.read=1U;
					myCharPtr[userDatalength+3]=myStatus.frame;
					osMessageQueuePut(queue_phyS_id, &fromQueueMacR, osPriorityNormal, osWaitForever);
					}
				}

		//If I am source of message => DATABACK (Check Read and Ack bits)
				if(srcAddr.myAddr.address == gTokenInterface.myAddress){
					fromQueueMacR.type = DATABACK;
					fromQueueMacR.addr = srcAddr.myAddr.address;//(*myCharPtr)>>3;
					fromQueueMacR.sapi = srcAddr.myAddr.sapi;//(*myCharPtr) & 0x07;
					osMessageQueuePut(queue_macS_id, &fromQueueMacR, osPriorityNormal, osWaitForever);
				}
		}
	}
}

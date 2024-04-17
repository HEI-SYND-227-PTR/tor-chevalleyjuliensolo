#include "stm32f7xx_hal.h"
#include <stdio.h>
#include <string.h>
#include "main.h"

void MacReceiver(void *argument)
{
	
	osStatus_t nextPhyRElem;
	
	struct queueMsg_t fromQueueMacR, toQueueMacS_Token;
	
	for (;;){
		nextPhyRElem = osMessageQueueGet(queue_macR_id, &fromQueueMacR,
			NULL,	osWaitForever);
		if(nextPhyRElem ==osOK){
		char* myCharPtr = fromQueueMacR.anyPtr;
		if((*myCharPtr)==0xFF){
				fromQueueMacR.type = TOKEN;
				osMessageQueuePut(queue_macS_id, &fromQueueMacR, osPriorityNormal, osWaitForever);
			}
			else if(((*myCharPtr)>>3)== gTokenInterface.myAddress){
				fromQueueMacR.type = DATABACK;
				fromQueueMacR.addr = (*myCharPtr)>>3;
				fromQueueMacR.sapi = (*myCharPtr) & 0x07;
				osMessageQueuePut(queue_macS_id, &fromQueueMacR, osPriorityNormal, osWaitForever);
				
			printf("Error Pool Alloc failed to give me some memory\n");
			}
			else if(((*(myCharPtr++))>>3) == gTokenInterface.myAddress){
				
				//osMessageQueuePut(
				
			}
			else{}
			
			
		}
	}
	//TO PHY Sender
	//TO MAC Sender 
			//Databack 
			//Token
	//TO Application Recievers
		
	// TODO
	}




#include <stdlib.h>
#include <time.h>

int density;
int position;
int spread;
int grainSize;
int grainCapacity;
int sampleSize;

int grainTimer = 2000000;

typedef struct Grain{
  int currentIndex;
  int end;
} Grain;

int grainTracker[10];

Grain grainArray[20];

int activeGrains = 0;
  
float r = 0.0;

int grn_pos;

int j;



void initializeGrainArray(){
    for (int i=0; i<20; i++){
	grainArray[i].end = -1;
    }
}

void setGranularParameters(int pDen, int pPos, int pSpr, int pGsz, int pQcap){
  density = pDen;
  position = pPos;
  if (pPos + pSpr < sampleSize){
      spread = pSpr;
  }else{
      spread = sampleSize - pPos;
  }
  grainSize = pGsz;
  grainCapacity = pQcap;
  
  
  //std::cout << "cap: " << queueCapacity << "\n";
}

void setSampleSize(int pSize){
    sampleSize = pSize;
}

int readyNextGrain(){
  
  if(grainTimer < density){
    grainTimer++;
    return 0;
  }
  else{
    grainTimer = 0;
    return 1;
  }
}

int grainQueueFull(){
    if(activeGrains < grainCapacity){
	return 0;
    }
    return 1;
}

void updateGrains(){
    //printf("trying to update grains\n");
    //printf(readyNextGrain());
    if(readyNextGrain() && !grainQueueFull()){
	int grainPos;
	//printf("making new grain\n");
	grainPos = rand() % spread + position;      

	for (int i=0; i < grainCapacity; i++){
	    if (grainArray[i].end == -1){
		grainArray[i].currentIndex = grainPos;//x2 because frame size is 2
		grainArray[i].end = grainSize;
		//printf("endindex: %i\n",grainArray[i].endIndex);
		
		activeGrains++;
		//printf("made Grain\n");
		break;
	    }
	}
    }
}

void updateOutBuffers(int i, float *sample_buffer, float *buffer_r){   

  updateGrains();
  int current_ag = activeGrains;
  if(current_ag > 0){
      
      for(j = 0; j < current_ag; j++){	
	  //grn = grainArray[j];
	  if(grainArray[j].end >= 0){
	      grn_pos = grainArray[j].currentIndex;

	      if (grn_pos == sampleSize){
		  grn_pos = 0;
	      }
	      
	      r += sample_buffer[grn_pos++];

	      grainArray[j].currentIndex = grn_pos;
	      grainArray[j].end--;
	      
	      if(grainArray[j].end <= 0){
		  grainArray[j].end = -1; //set the end Index to -1 so system knows grain j is inactive
		  activeGrains--;
	      }
	  }
      }
     
      buffer_r[i] = r/current_ag;//divide the aggragate signal by the number of active grains
      //reset l/r
      r = 0.0;
    }
    // if there are no active grains, set the output of that frame to zero
    else{
      buffer_r[i] = 0.0;
      
    } 

  
}

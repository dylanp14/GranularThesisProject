#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <sndfile.h>
#include <jack/jack.h>
#include <jack/midiport.h>

#include "grain_handler.h"
#include "pitch.h"

jack_port_t **output_ports;
jack_port_t *input_port;
jack_client_t *client;

SNDFILE *wav;
SF_INFO wavInfo;

jack_default_audio_sample_t big_buffer[882000];
jack_default_audio_sample_t mono_buffer[441000];
jack_default_audio_sample_t *buffer_r;
jack_default_audio_sample_t *buffer_l;

int big_buffer_index = 0;

int mdensity;
int mposition;
int mspread;
int mgrain_size;
int mqueue_capacity;
int ssize;
int channels;

jack_default_audio_sample_t note_on;
unsigned char note = 1;
float noteArray[128];

//void smbPitchShift(float, long, long, long, float, float*, float*);

static void signal_handler(int sig)
{
    jack_client_close(client);
    fprintf(stderr, "signal recieved, exiting ...\n");
    exit(0);
}

void initializeNotes(){
    int x = -64;
    for(int i=0; i<128; i++){
	noteArray[i] = pow(pow(2.f,1.f/12.f),x);
	x++;
    }
}

int process (jack_nframes_t nframes, void *arg)
{
    int i;
    jack_default_audio_sample_t *out1, *out2;
   
    
    out1 = (float*)jack_port_get_buffer(output_ports[0], nframes);
    out2 = (float*)jack_port_get_buffer(output_ports[1], nframes);

    
    void* port_buf = jack_port_get_buffer(input_port, nframes);
   
    jack_midi_event_t in_event;
    jack_nframes_t event_index = 0;
    jack_nframes_t event_count = jack_midi_get_event_count(port_buf);

    if(event_count >= 1)	{
	//printf(" granular_client: have %d events\n", event_count);

	for(i=0; i<event_count; i++){
	    jack_midi_event_get(&in_event, port_buf, i);
	}
/*		printf("1st byte of 1st event addr is %p\n", in_events[0].buffer);*/
    }


    for(i=0; i<nframes; i++){
	if ((in_event.time == i) && (event_index < event_count)){
	    if (((*(in_event.buffer) & 0xf0)) == 0x90){
		// note on
		note = *(in_event.buffer + 1);
		//printf("note: %i\n", note);
		if (*(in_event.buffer + 2) == 0) {
		    note_on = 0.0;
		} else {
		    note_on = (float)(*(in_event.buffer + 2)) / 127.f;
		}
	    }
	    else if (((*(in_event.buffer))) == 0xb0){
		if(((*(in_event.buffer+1))) == 0x16){
		    //printf("%f\n", ((float)ssize/127.0)*(float)(*(in_event.buffer+2)));
		    
		    mposition = (int)(((float)ssize/127.0) * ((float)(*(in_event.buffer+2)+1.0)))-1;
		    //printf("position: %i\n", mposition);

		}
		else if(((*(in_event.buffer+1))) == 0x17){
		    mspread = (int)(((float)ssize/128.0) * ((float)(*(in_event.buffer+2)+1.0)));
		    //printf("spread: %i\n", mspread);
		}
		else if(((*(in_event.buffer+1))) == 0x18){
		    mdensity = (int)(((float)44100/128.0) * ((float)(*(in_event.buffer+2)+1.0)));
		    //printf("density: %i\n", mdensity);
		}
		else if(((*(in_event.buffer+1))) == 0x19){
		    mgrain_size = (int)(((float)44100/128.0) * ((float)(*(in_event.buffer+2)+1.0)));
		    //printf("grain size: %i\n", mgrain_size);
		}
	    }
		
	
	    else if (((*(in_event.buffer)) & 0xf0) == 0x80){
		// note off
		//note = *(in_event.buffer + 1);
		note_on = 0.0;
	    }
	    event_index++;
	    if(event_index < event_count){
		jack_midi_event_get(&in_event, port_buf, event_index);
	    }
	    
	    	
	       
        }

	setGranularParameters(mdensity, mposition, mspread, mgrain_size, mqueue_capacity);
	
	updateOutBuffers(i, mono_buffer, buffer_r);
    }

    smbPitchShift(noteArray[note], nframes, 1024, 6, 44100, buffer_r, out1);
    //smbPitchShift(noteArray[note], 512, nframes, 4, 44100, buffer_l, out2);
    memcpy( out2, out1, nframes * sizeof (jack_default_audio_sample_t));
    //updateOutBuffers(nframes, big_buffer, out1, out2);
    
    
    return 0;
}

void jack_shutdown(void *arg)
{
    free(output_ports);
    exit(1);
}

int main(int argc, char *argv[])
{
    int i;
    const char **ports;
    const char *client_name;
    const char *server_name;
    jack_options_t options = JackNullOption;
    jack_status_t status;

    mdensity = 44100;
    mposition = 0;
    mspread = 1;
    mgrain_size = 44100;
    mqueue_capacity = 10;

    setGranularParameters(mdensity, mposition, mspread, mgrain_size, mqueue_capacity);

    initializeGrainArray();
    initializeNotes();
    
    buffer_r = (float*)malloc(sizeof(jack_default_audio_sample_t)*441000);

    if (argc >= 2)
    {
	client_name = argv[1];
	/*if (argc >= 3)
	{
	    server_name = argv[2];
	    options |= JackServerName;
	    }*/
    }
    else
    {
	client_name =strchr (argv[0], '/');
	if (client_name == 0)
	{
	    client_name = argv[0];
	}
        else
	{
	    client_name++;
	}
    }
    //load wav file
    if (!(wav = sf_open("guitar.wav", SFM_READ, &wavInfo))){
	exit(0);
    }
    //transfer into buffer
    if(!sf_read_float(wav, big_buffer, wavInfo.frames*2)){
	exit(0);
    }
    printf("channels: %i\n", wavInfo.channels);
    
    ssize = wavInfo.frames;
    channels = wavInfo.channels;
    
    setSampleSize(ssize);
    
    // de-interleave wav data and convert to mono  
    int ridx = 0;    
    for(i = 0; i<wavInfo.frames*channels; i++){
	mono_buffer[ridx++] = big_buffer[i++];
    }
  

    client = jack_client_open(client_name, options, &status, server_name);
    if (client == NULL)
    {
	fprintf(stderr, "jack_client_open() failed, "
      	    "status  - 0x%2.0x\n", status );
	if (status & JackServerFailed)
        {
	    fprintf( stderr, "Unable to connect to Jack server\n");
	}
        exit(1);
    }
    if (status & JackServerStarted)
    {
	fprintf(stderr, "Jack server started\n");
    }
    if (status & JackNameNotUnique)
    {
	client_name = jack_get_client_name(client);
        fprintf(stderr, "unique name '%s' assigned\n");
    }

    jack_set_process_callback(client, process, 0);

    jack_on_shutdown(client, jack_shutdown, 0);

    output_ports = (jack_port_t**) calloc(2, sizeof(jack_port_t*));

    char port_name[16];
    for (i=0; i<2; i++){
	sprintf(port_name, "output_%d", i+1);
	output_ports[i] = jack_port_register(client, port_name, JACK_DEFAULT_AUDIO_TYPE,
					     JackPortIsOutput, 0);
	if((output_ports[i] == NULL)){
	    fprintf(stderr, "no more JACK ports available\n");
	    exit(1);
	}
    }
    input_port = jack_port_register(client, "midi_in", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);

    if(jack_activate(client)){
	fprintf(stderr, "cannot activate client");
	exit(1);
    }

    ports = jack_get_ports(client, NULL, NULL, JackPortIsPhysical|JackPortIsInput);
    if(ports == NULL){
	fprintf(stderr, "no physical playback ports\n");
	exit(1);
    }

    for(i=0; i<2; i++)
	if(jack_connect(client, jack_port_name(output_ports[i]), ports[i]))
	    fprintf(stderr, "cannot connect input ports\n");

    free(ports);

    #ifdef WIN32
    signal(SIGNIT, signal_handler);
    signal(SIGABRT, signal_handler);
    signal(SIGTERM, signal_handler);
    #else
    signal(SIGQUIT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGINT, signal_handler);
    #endif

    while(1) {
	#ifdef WIN32
	sleep(1000);
	#else
	//sleep(1);
	#endif
    }

    jack_client_close(client);
    exit(0);
}

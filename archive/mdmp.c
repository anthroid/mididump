//
//	mdmp.c
//
//	Displays incoming MIDI messages
//
//	Created by Devin Palmer on 3/31/13.
//
//	To do: Remove printf calls from MIDI callback, modify callback to queue message
//	to be printed by main process (or another thread), so as to not call blocking 
//	functions from within the callback. Add message queueing mechanisms to support.
//
//	gcc mdmp.c -o bin/mdmp -framework CoreMIDI -framework CoreServices

#include <CoreMIDI/MIDIServices.h>
#include <CoreServices/CoreServices.h>
#include <stdio.h>
#include <signal.h>

//	global constants

#define SOURCE_NAME_STR_LEN 128
#define MAX_NUM_SOURCES 16

//	structure declarations

struct mdmp_endpoint {
	MIDIEndpointRef endpoint;
	char *description;
};

struct mdmp_format {
	int source_col_len;
};

struct mdmp {
	MIDIClientRef client;
	MIDIPortRef port;
	struct mdmp_endpoint *sources;
	struct mdmp_format format;
};

//	function prototypes

void mdmp_midi_notify_proc(const MIDINotification *message, void *sender);
void mdmp_midi_read_proc(const MIDIPacketList *list, void *sender, void *source);
void mdmp_init(struct mdmp *sender);
void mdmp_update(struct mdmp *sender);

//	function definitions

void mdmp_midi_notify_proc(const MIDINotification *message, void *sender) {
	struct mdmp *mdmp_ptr = (struct mdmp *)sender;
	switch (message->messageID)
	{
			case 1:	mdmp_update(mdmp_ptr);
					break;
	}
}

void mdmp_midi_read_proc(const MIDIPacketList *list, void *sender, void *source) {
	int i, j;
	if (list->numPackets > 0) {
		printf("%-*s: ", ((struct mdmp*)sender)->format.source_col_len, ((struct mdmp_endpoint*)source)->description);
		for (i = 0; i < list->numPackets; i++) {
			if (list->packet[i].length > 0) {
				for (j = 0; j < list->packet[i].length; j++) {
					printf("%2x ", list->packet[i].data[j]);
				}
				printf("\n");
			}
		}
	}
}

void mdmp_init(struct mdmp *sender) {
	MIDIClientCreate(CFSTR("mdmp"), mdmp_midi_notify_proc, (void*)sender, &(sender->client));
	MIDIInputPortCreate(sender->client, CFSTR("IN"), mdmp_midi_read_proc, (void*)sender, &(sender->port));
	sender->format.source_col_len = 0;
	mdmp_update(sender);
}

void mdmp_update(struct mdmp *sender) {
	int i = 0, len = 0, num_sources = MIDIGetNumberOfSources();
	sender->sources = malloc(num_sources * sizeof(struct mdmp_endpoint));
	CFStringRef cfstr;
	char *cstr = malloc(SOURCE_NAME_STR_LEN * sizeof(char));
	
	if (num_sources < 1) {
		printf("No MIDI sources available.\n");
	}
	else {
		for (i = 0; i < num_sources; i++) {
			sender->sources[i].endpoint = MIDIGetSource(i);
			sender->sources[i].description = malloc(SOURCE_NAME_STR_LEN * sizeof(char));
			
			MIDIObjectGetStringProperty(sender->sources[i].endpoint, kMIDIPropertyDisplayName, &cfstr);
			CFStringGetCString(cfstr, sender->sources[i].description, SOURCE_NAME_STR_LEN, kCFStringEncodingASCII);
			
			len = strlen(sender->sources[i].description);
			if (len > sender->format.source_col_len) {
				if (len > SOURCE_NAME_STR_LEN) {
					sender->format.source_col_len = SOURCE_NAME_STR_LEN;
				} else {
					sender->format.source_col_len = len;
				}
			}
			
			MIDIPortConnectSource(sender->port, sender->sources[i].endpoint, (void*)&(sender->sources[i]));
			printf("Connected source %i: %s\n", i, sender->sources[i].description);
		}
	}
}

void mdmp_handle_sigint(int s) {
	printf("\e[1;1H\e[2J");
	fflush(stdout);
}

int main(int argc, char **argv) {
	struct mdmp self;
	
	mdmp_init(&self);
	printf("Press Ctrl-C to clear, Ctrl-\\ to quit\n");
	signal(SIGINT, mdmp_handle_sigint);
	
	CFRunLoopRun();
	return 0;
}

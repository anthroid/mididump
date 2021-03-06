//
//	mididump.c
//
//	Command line utility to display incoming MIDI messages

#include <CoreMIDI/MIDIServices.h>
#include <CoreServices/CoreServices.h>
#include <stdio.h>
#include <unistd.h>

//	Macro definitions
#define unless(x) if (!(x))

//	Global constants
#define SOURCE_NAME_MAX_STRLEN 255
#define ESC_COLOR_MIDI_NOTE_ON "\033[32m"
#define ESC_COLOR_MIDI_NOTE_OFF "\033[35m"
#define ESC_COLOR_MIDI_CC "\033[36m"
#define ESC_COLOR_MIDI_PB "\033[93m"
#define ESC_COLOR_MIDI_AT "\033[94m"
#define ESC_COLOR_SOURCE "\033[36m"
#define ESC_COLOR_RESET "\033[0m"
#define ESC_CLEAR_OUTPUT "\e[1;1H\e[2J"

//	Structure declarations

//	Command line options
typedef struct {
	uint8_t opt_s, opt_c, opt_d, opt_m, opt_n, opt_t, opt_x, opt_z;
} mdmp_options_t;

//	Endpoint references
typedef struct {
	MIDIEndpointRef endpoint;
	char *description;
	int id;
} mdmp_endpoint_t;

//	Output format options
typedef struct {
	uint8_t source_col_len;
	uint8_t source_num_len;
	uint8_t single_line;
	uint8_t color_output;
	uint8_t decimal;
	uint8_t source_number;
	uint8_t source_name;
	uint8_t timestamp;
	uint8_t zero_prefix;
} mdmp_format_t;

//	Application context
typedef struct {
	MIDIClientRef client;
	MIDIPortRef port;
	mdmp_options_t options;
	mdmp_endpoint_t *sources;
	mdmp_format_t format;
	int16_t num_sources;
} mdmp_context_t;

//	Function prototypes
void mdmp_midi_notify_proc(const MIDINotification *message, void *sender);
void mdmp_midi_read_proc(const MIDIPacketList *list, void *sender, void *source);
void mdmp_init(mdmp_context_t *sender);
void mdmp_config(int argc, char **argv, mdmp_context_t *sender);
void mdmp_update(mdmp_context_t *sender);
void mdmp_handle_sigint(int s);
void mdmp_usage(void);

//	Function definitions

void mdmp_print_timestamp() {
	struct timespec ts;
	struct tm *tm;
	clock_gettime(CLOCK_REALTIME, &ts);
	tm = localtime(&ts.tv_sec);
	printf("%02d:%02d:%02d.%06ld  ",
		tm->tm_hour,
		tm->tm_min,
		tm->tm_sec,
		ts.tv_nsec / 1000
	);
}

//	CoreMIDI notification callback
void mdmp_midi_notify_proc(const MIDINotification *message, void *sender) {
	mdmp_context_t *self = (mdmp_context_t *)sender;
	switch (message->messageID)
	{
			case kMIDIMsgSetupChanged:
				mdmp_update(self);
				break;
	}
}

//	CoreMIDI read callback
void mdmp_midi_read_proc(const MIDIPacketList *list, void *sender, void *source) {
	int i, j;
	mdmp_context_t *context = (mdmp_context_t*)sender;
	mdmp_endpoint_t *endpoint = (mdmp_endpoint_t*)source;
	
	if (list->numPackets > 0) {
		
		//	Clear screen if single line option is enabled
		if (context->format.single_line) {
			printf(ESC_CLEAR_OUTPUT);
		}
		
		//	Print timestamp
		if (context->format.timestamp) {
			mdmp_print_timestamp();
		}
		
		//	Print source number
		if (context->format.source_number) {
			if (context->format.color_output) {
				printf("%s%*d%s: ",
					ESC_COLOR_SOURCE,
					context->format.source_num_len,
					endpoint->id,
					ESC_COLOR_RESET
				);
			} else {
				printf("%*d: ", context->format.source_num_len, endpoint->id);
			}
		}
		
		//	Print source name
		if (context->format.source_name) {
			printf("%-*s: ", context->format.source_col_len, endpoint->description);
		}
		
		//	Print formatted packet data
		for (i = 0; i < list->numPackets; i++) {
			if (list->packet[i].length > 0) {
				for (j = 0; j < list->packet[i].length; j++) {
					//	Color status bytes if option is enabled
					if (context->format.color_output) {
						switch (list->packet[i].data[j] & 0xF0) {
							case 0x90:
								printf(ESC_COLOR_MIDI_NOTE_ON);
								break;
							case 0x80:
								printf(ESC_COLOR_MIDI_NOTE_OFF);
								break;
							case 0xb0:
								printf(ESC_COLOR_MIDI_CC);
								break;
							case 0xd0:
								printf(ESC_COLOR_MIDI_AT);
								break;
							case 0xe0:
								printf(ESC_COLOR_MIDI_PB);
								break;
							default:
								printf(ESC_COLOR_RESET);
								break;
						}
					}
					//	Print byte to terminal
					if (context->format.zero_prefix) {
						//	Print zero prefixed
						if (context->format.decimal) {
							//	Zero prefixed, decimal
							printf("%03d ", list->packet[i].data[j]);
						} else {
							//	Zero prefixed, hexadecimal
							printf("%02x ", list->packet[i].data[j]);
						}
					} else {
						//	Print right aligned
						if (context->format.decimal) {
							//	Right aligned, decimal
							printf("%3d ", list->packet[i].data[j]);
						} else {
							//	Right aligned, hexadecimal
							printf("%2x ", list->packet[i].data[j]);
						}
					}
				}
				printf("\n");
			}
		}
	}
}

//	Initialize MIDI ports
void mdmp_init(mdmp_context_t *sender) {
	//	Create MIDI client
	MIDIClientCreate(CFSTR("mdmp"), mdmp_midi_notify_proc,
		(void*)sender,
		&(sender->client)
	);
	
	//	Create input port
	MIDIInputPortCreate(sender->client, CFSTR("IN"), mdmp_midi_read_proc,
		(void*)sender,
		&(sender->port)
	);
	
	//	Initialize context variables
	sender->format.source_num_len = 0;
	sender->format.source_num_len = 0;
	sender->num_sources = 0;
	
	//	Enumerate MIDI ports
	mdmp_update(sender);
}

void mdmp_config(int argc, char **argv, mdmp_context_t *sender) {
	int i;
	
	//	Exit if wrong argument count
	if (argc > 2) {
		mdmp_usage();
		exit(-1);
	}
	
	//	Initialize options data structure
	memset((void*)&sender->options, 0, sizeof(mdmp_options_t));
	
	//	Parse command line options
	while ((i = getopt(argc, argv, "scdmntxzh")) != -1) {
		switch (i) {
			case 's':
				sender->options.opt_s = 1;
				break;
			case 'c':
				sender->options.opt_c = 1;
				break;
			case 'd':
				sender->options.opt_d = 1;
				break;
			case 'm':
				sender->options.opt_m = 1;
				break;
			case 'n':
				sender->options.opt_n = 1;
				break;
			case 't':
				sender->options.opt_t = 1;
				break;
			case 'x':
				sender->options.opt_x = 1;
				break;
			case 'z':
				sender->options.opt_z = 1;
				break;
			case 'h':
				mdmp_usage();
				exit(0);
			case '?':
				switch (optopt) {
					default:
						printf("Unknown option '%c'\n", optopt);
						break;
				}
				exit(-1);
			default:
				printf("Error parsing command line options\n");
				mdmp_usage();
				exit(-1);
		}
	}
	
	//	Set default output format if invoked without any display options
	if ((
		sender->options.opt_s | sender->options.opt_c | 
		sender->options.opt_d | sender->options.opt_m |
		sender->options.opt_n | sender->options.opt_t |
		sender->options.opt_z) == 0) {
		sender->options.opt_c = 1;
		sender->options.opt_n = 1;
	}
	
	//	Set extended format
	if (sender->options.opt_x) {
		sender->format.color_output = 1;
		sender->format.source_number = 1;
		sender->format.source_name = 1;
		sender->format.timestamp = 1;
		sender->format.zero_prefix = 1;
	}
	
	//	Single line output
	if (sender->options.opt_s) {
		sender->format.single_line = 1;
	}
	
	//	Color output
	if (sender->options.opt_c) {
		sender->format.color_output = 1;
	}
	
	//	Decimal output
	if (sender->options.opt_d) {
		sender->format.decimal = 1;
	}
	
	//	Source number
	if (sender->options.opt_m) {
		sender->format.source_number = 1;
	}
	
	//	Source name
	if (sender->options.opt_n) {
		sender->format.source_name = 1;
	}
	
	//	Timestamp
	if (sender->options.opt_t) {
		sender->format.timestamp = 1;
	}
	
	//	Zero prefix
	if (sender->options.opt_z) {
		sender->format.zero_prefix = 1;
	}
}

//	Update MIDI source list
void mdmp_update(mdmp_context_t *sender) {
	int i = 0, j = 0, len = 0;
	CFStringRef cfstr;
	
	//	Disconnect and initialize any previously allocated sources
	for (i = 0; i < sender->num_sources; i++) {
		if (sender->sources[i].description != NULL) {
			free(sender->sources[i].description);
			sender->sources[i].description = NULL;
		}
		if (sender->sources[i].endpoint) {
			MIDIPortDisconnectSource(sender->port, sender->sources[i].endpoint);
			sender->sources[i].endpoint = 0;
		}
		sender->sources[i].id = 0;
	}
	
	//	Allocate memory for new source list
	sender->num_sources = MIDIGetNumberOfSources();
	sender->sources = realloc(sender->sources, sender->num_sources * sizeof(mdmp_endpoint_t));
	memset((void*)sender->sources, 0, sender->num_sources * sizeof(mdmp_endpoint_t));
	
	//	Build MIDI source list and connect to each source
	if (sender->num_sources < 1) {
		printf("No MIDI sources available.\n");
		return;
	}
	else {
		//	Set source number column width
		sender->format.source_num_len =
			(sender->num_sources < 10) ? 1 : (sender->num_sources < 100) ? 2 : 3;
		
		//	Configure and connect each source
		for (i = 0; i < sender->num_sources; i++) {
			sender->sources[i].endpoint = MIDIGetSource(i);
			sender->sources[i].description = malloc(SOURCE_NAME_MAX_STRLEN * sizeof(char));
			sender->sources[i].id = i;
			
			MIDIObjectGetStringProperty(sender->sources[i].endpoint, kMIDIPropertyName, &cfstr);
			CFStringGetCString(cfstr, 
				sender->sources[i].description,
				SOURCE_NAME_MAX_STRLEN,
				kCFStringEncodingASCII
			);
			
			//	Adjust source name column to length of longest source name
			len = strnlen(sender->sources[i].description, SOURCE_NAME_MAX_STRLEN);
			if (len > sender->format.source_col_len) {
				if (len > SOURCE_NAME_MAX_STRLEN) {
					sender->format.source_col_len = SOURCE_NAME_MAX_STRLEN;
				} else {
					sender->format.source_col_len = len;
				}
			}
			
			MIDIPortConnectSource(
				sender->port,
				sender->sources[i].endpoint,
				(void*)&(sender->sources[i])
			);
		}
	}
	
	//	Check for uniqueness, prepend kMIDIPropertyModel if a match is found
	for (i = 0; i < sender->num_sources; i++) {
		for (j = i + 1; j < sender->num_sources; j++) {
			if (strncmp(sender->sources[i].description, sender->sources[j].description,
					SOURCE_NAME_MAX_STRLEN) == 0) {
				char *a, *b;
				
				a = malloc(SOURCE_NAME_MAX_STRLEN * sizeof(char));
				b = strndup(sender->sources[i].description, SOURCE_NAME_MAX_STRLEN);
				
				MIDIObjectGetStringProperty(
					sender->sources[i].endpoint, kMIDIPropertyModel, &cfstr);
				CFStringGetCString(
					cfstr, a, SOURCE_NAME_MAX_STRLEN, kCFStringEncodingASCII);
				snprintf(sender->sources[i].description,
					SOURCE_NAME_MAX_STRLEN, "%s %s", a, b);
				
				free(a);
				free(b);
				
				//	Adjust source name column to length of longest source name
				len = strnlen(sender->sources[i].description, SOURCE_NAME_MAX_STRLEN);
				if (len > sender->format.source_col_len) {
					if (len > SOURCE_NAME_MAX_STRLEN) {
						sender->format.source_col_len = SOURCE_NAME_MAX_STRLEN;
					} else {
						sender->format.source_col_len = len;
					}
				}
			}
		}
	}
	
	//	List all MIDI sources
	for (i = 0; i < sender->num_sources; i++) {
		printf(
			"Connected source %i: %s\n", 
			sender->sources[i].id,
			sender->sources[i].description
		);
	}
}

//	Print usage
void mdmp_usage(void) {
	printf(
		"Usage:\n"
		"-s  Single line\n"
		"-c  Color\n"
		"-d  Decimal format\n"
		"-m  Source number\n"
		"-n  Source name\n"
		"-t  Timestamp\n"
		"-z  Zero prefix\n"
		"-h  Show command help\n"
	);
}

//	Main application loop
int main(int argc, char **argv) {
	mdmp_context_t self;
	mdmp_config(argc, argv, &self);
	mdmp_init(&self);
	printf("Press Ctrl-C to quit, Cmd-K to clear\n");
	CFRunLoopRun();
	return 0;
}

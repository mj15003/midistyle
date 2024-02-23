#include <alsa/asoundlib.h>
#include <smf.h>

#define LINE_LENGTH 127
#define CHS_LENGTH 8
#define CHT_LENGTH 6

#define INTRO_1 0x00
#define INTRO_2 0x01
#define INTRO_3 0x03
#define MAIN_A 0x08
#define MAIN_B 0x09
#define MAIN_C 0x0A
#define MAIN_D 0x0B
#define FILL_IN_AA 0x10
#define FILL_IN_BB 0x11
#define FILL_IN_CC 0x12
#define FILL_IN_DD 0x13
#define BREAK_FILL 0x18
#define ENDING_1 0x20
#define ENDING_2 0x21
#define ENDING_3 0x22
#define ENDING_4 0x23
#define SWITCH_ON 0x7F

#define ROOT_FLAT 0x20
#define ROOT_NATURAL 0x30
#define ROOT_SHARP 0x40

#define ROOT_C 0x01
#define ROOT_D 0x02
#define ROOT_E 0x03
#define ROOT_F 0x04
#define ROOT_G 0x05
#define ROOT_A 0x06
#define ROOT_B 0x07

#define CHORD_MAJ 0x00
#define CHORD_MAJ6 0x01
#define CHORD_MAJ7 0x02
#define CHORD_ADD9 0x04
#define CHORD_AUG 0x07
#define CHORD_MIN 0x08
#define CHORD_MIN6 0x09
#define CHORD_MIN7 0x0A
#define CHORD_DIM 0x11
#define CHORD_DIM7 0x12
#define CHORD_7TH 0x13
#define CHORD_7SUS4 0x14
#define CHORD_1_5 0x1F
#define CHORD_SUS4 0x20
#define CHORD_SUS2 0x21
#define CHORD_CC 0x22

typedef enum { Section, Chord, Tempo, TSig, Stop } Template_t;

static uint8_t section[] = {
   0xf0,0x43,0x7e,0x00,0x00,0x7f,0xf7 //SECTION TEMPLATE
};

static uint8_t chord[] = {
   0xf0,0x43,0x7e,0x02,0x31,0x00,0x7f,0x7f,0xf7 //CHORD TEMPLATE
};

static uint8_t tempo[] = {
   0xff,0x51,0x03,0x09,0x89,0x68 //Tempo Metaevent 96 BPM
};

static uint8_t tsignature[] = {
   0xff,0x58,0x04,0x04,0x02,0x18,0x08 //Time Signature Metaevent 4/4 24 8
};

static char *infilename;
static char *outfilename;
static FILE *infile;

static char *tempo_override = NULL;
static uint8_t tpchn = 0;	//tempo changes number

static char line[LINE_LENGTH];
static char chs[CHS_LENGTH];
static char cht[CHT_LENGTH];

static Template_t type;

static uint8_t offset;

static smf_t *smf;
static smf_track_t *track;
static smf_event_t *event;
static smf_tempo_t *smf_tempo;
static uint32_t smf_length;

static snd_seq_t *seq;
static int client;
static int queue;
static char *addr_s;
static snd_seq_event_t ev;

static void set_tempo_bytes(char *tv)
{
	uint32_t bpm = atoi(tv);
	uint32_t tuspqn = 60000000/bpm; // convert to microseconds per quater note
	tempo[3] = tuspqn >> 16; 
	tempo[4] = (tuspqn & 0xFF00) >> 8;
	tempo[5] = tuspqn & 0xFF;
}

static int process_line() 
{
	int itm = sscanf(line,"%3hhu %7s",&offset,chs);
	if (itm > 1) {
		if (strcmp(chs,"BREAK") == 0) {
			type = Section;
			section[4] = BREAK_FILL;
		}
		else if (strcmp(chs,"INTRO1") == 0) {
			type = Section;
			section[4] = INTRO_1;
		}
		else if (strcmp(chs,"INTRO2") == 0) {
			type = Section;
			section[4] = INTRO_2;
		}
		else if (strcmp(chs,"INTRO3") == 0) {
			type = Section;
			section[4] = INTRO_3;
		}
		else if (strcmp(chs,"FILLINA") == 0) {
			type = Section;
			section[4] = FILL_IN_AA;
		}
		else if (strcmp(chs,"FILLINB") == 0) {
			type = Section;
			section[4] = FILL_IN_BB;
		}
		else if (strcmp(chs,"FILLINC") == 0) {
			type = Section;
			section[4] = FILL_IN_CC;
		}
		else if (strcmp(chs,"FILLIND") == 0) {
			type = Section;
			section[4] = FILL_IN_DD;
		}
		else if (strcmp(chs,"MAIN_A") == 0) {
			type = Section;
			section[4] = MAIN_A;
		}
		else if (strcmp(chs,"MAIN_B") == 0) {
			type = Section;
			section[4] = MAIN_B;
		}
		else if (strcmp(chs,"MAIN_C") == 0) {
			type = Section;
			section[4] = MAIN_C;
		}
		else if (strcmp(chs,"MAIN_D") == 0) {
			type = Section;
			section[4] = MAIN_D;
		}
		else if (strcmp(chs,"ENDING1") == 0) {
			type = Section;
			section[4] = ENDING_1;
		}
		else if (strcmp(chs,"ENDING2") == 0) {
			type = Section;
			section[4] = ENDING_2;
		}
		else if (strcmp(chs,"ENDING3") == 0) {
			type = Section;
			section[4] = ENDING_3;
		}
		else if (strcmp(chs,"ENDING4") == 0) {
			type = Section;
			section[4] = ENDING_4;
		}
		else if (strcmp(chs,"STOP") == 0) {
			type = Stop;
		}
		else if (chs[0] == 'T') {
			if (chs[1] == 'S') {
				type = TSig;
				tsignature[3] = chs[2] - '0';//nominator
				if (chs[3] == '8')
					tsignature[4] = 3;//denumerator
				else
					tsignature[4] = 2;//denumerator
			} else {
				type = Tempo;
				strcpy(cht,&chs[1]);  //get substring from 2nd character to end
				set_tempo_bytes(cht);
			}
		} else {
			//CHORD
			type = Chord;

			if (chs[0] == 'D') chord[4] = ROOT_D;
			else if (chs[0] == 'E') chord[4] = ROOT_E;
			else if (chs[0] == 'F') chord[4] = ROOT_F;
			else if (chs[0] == 'G') chord[4] = ROOT_G;
			else if (chs[0] == 'A') chord[4] = ROOT_A;
			else if (chs[0] == 'B') chord[4] = ROOT_B;
			else chord[4] = ROOT_C;

			if (chs[1] == '#') {
				chord[4] = chord[4] | ROOT_SHARP;
				strcpy(cht,&chs[2]);  //get substring from 3rd character to end
			}
			else if (chs[1] == 'b') {
				chord[4] = chord[4] | ROOT_FLAT;
				strcpy(cht,&chs[2]);  //get substring from 3rd character to end
			}
			else {
				chord[4] = chord[4] | ROOT_NATURAL;
				strcpy(cht,&chs[1]);  //get substring from 2nd character to end
			}

			if (cht[0] == 0) chord[5] = CHORD_MAJ;
			else if (strcmp(cht,"M7") == 0) chord[5] = CHORD_MAJ7;
			else if (strcmp(cht,"m6") == 0) chord[5] = CHORD_MIN6;
			else if (strcmp(cht,"m7") == 0) chord[5] = CHORD_MIN7;
			else if (strcmp(cht,"add9") == 0) chord[5] = CHORD_ADD9;
			else if (strcmp(cht,"aug") == 0) chord[5] = CHORD_AUG;
			else if (strcmp(cht,"dim") == 0) chord[5] = CHORD_DIM;
			else if (strcmp(cht,"dim7") == 0) chord[5] = CHORD_DIM7;
			else if (strcmp(cht,"7sus4") == 0) chord[5] = CHORD_7SUS4;
			else if (cht[0] == 'm') chord[5] = CHORD_MIN;
			else if (cht[0] == '2') chord[5] = CHORD_SUS2;
			else if (cht[0] == '4') chord[5] = CHORD_SUS4;
			else if (cht[0] == '5') chord[5] = CHORD_1_5;
			else if (cht[0] == '6') chord[5] = CHORD_MAJ6;
			else if (cht[0] == '7') chord[5] = CHORD_7TH;
			else chord[5] = CHORD_CC;
		}
	}
	return itm;
}

static void show_usage()
{
	puts("\nmidistyle -i input-file-name -p output midi port|-o output-file-name\n");
	puts("   -h help");
	puts("   -l list ALSA MIDI ports\n");
}

void signal_handler(int sig)
{
	fprintf(stderr, "\nsignal received, exiting ...\n");
	if (outfilename == NULL) {

		snd_seq_stop_queue(seq, queue, NULL);
		snd_seq_drop_output(seq);

		snd_seq_ev_clear(&ev);
		snd_seq_ev_set_source(&ev, 0);
		snd_seq_ev_set_subs(&ev);

		ev.type = SND_SEQ_EVENT_STOP;

		snd_seq_ev_set_direct(&ev);
		snd_seq_event_output_direct(seq, &ev);

		snd_seq_close(seq);

		smf_delete(smf);

	}
	exit(EXIT_SUCCESS);
}

static void list_ports(void)
{

	snd_seq_client_info_t *cinfo;
	snd_seq_port_info_t *pinfo;

	snd_seq_client_info_alloca(&cinfo);
	snd_seq_port_info_alloca(&pinfo);

	puts(" Port    Client name                      Port name");

	snd_seq_client_info_set_client(cinfo, -1);
	while (snd_seq_query_next_client(seq, cinfo) >= 0) {
		int client = snd_seq_client_info_get_client(cinfo);

		snd_seq_port_info_set_client(pinfo, client);
		snd_seq_port_info_set_port(pinfo, -1);
		while (snd_seq_query_next_port(seq, pinfo) >= 0) {
			/* port must understand MIDI messages */
			if (!(snd_seq_port_info_get_type(pinfo)
		     & SND_SEQ_PORT_TYPE_MIDI_GENERIC))
		continue;
			/* we need both WRITE and SUBS_WRITE */
			if ((snd_seq_port_info_get_capability(pinfo)
		    & (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE))
		   != (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE))
		continue;
			printf("%3d:%-3d  %-32.32s %s\n",
		      snd_seq_port_info_get_client(pinfo),
		      snd_seq_port_info_get_port(pinfo),
		      snd_seq_client_info_get_name(cinfo),
		      snd_seq_port_info_get_name(pinfo));
		}
	}
}

/* error handling for ALSA functions */
static void check_snd(const char *operation, int err)
{
	if (err < 0) {
		printf("Cannot %s - %s", operation, snd_strerror(err));
		exit(EXIT_FAILURE);
	}
}

static void init_seq(void)
{
	int err;

	/* open sequencer */
	err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
	check_snd("open sequencer", err);

	/* set our name (otherwise it's "Client-xxx") */
	err = snd_seq_set_client_name(seq, "midistyle");
	check_snd("set client name", err);

	/* find out who we actually are */
	client = snd_seq_client_id(seq);
	check_snd("get client id", client);
}

static void init_smf()
{
	/* Create SMF MIDI file structures */
	smf = smf_new();
	if (smf == NULL) {
		perror("Could not create a new SMF file structure.");
		exit(EXIT_FAILURE);
	}
	track = smf_track_new();
	smf_add_track(smf,track);

	event = smf_event_new_from_pointer(tsignature,7);
	smf_track_add_event_pulses(track,event,0);

	if (tempo_override != NULL) {
		set_tempo_bytes(tempo_override);
		event = smf_event_new_from_pointer(tempo,6);
		smf_track_add_event_pulses(track,event,0);
	}

	if (outfilename != NULL) {
		/* Generate midi 1 bar count in at the begining
			in order for output file to be playable on Keyboard.
			It must contain at least some regular notes.*/
		for (uint32_t p=0; p < 4*smf->ppqn; p+=smf->ppqn) {
			event = smf_event_new_from_bytes(0x99,56,1);	//Channel 10, Cowbell, Velocity=1;
			smf_track_add_event_pulses(track,event,p);

			event = smf_event_new_from_bytes(0x89,56,0);	//Note Off
			smf_track_add_event_pulses(track,event,p+smf->ppqn/2);
		}

		/* one more artificial note off at begining of second bar 
			to get correct delta offsets */
		event = smf_event_new_from_bytes(0x89,56,0);	//Note Off
		smf_track_add_event_pulses(track,event,4*smf->ppqn);
	}
}

static void add_midi_file_event()
{
	if (type == Section) {

		event = smf_event_new_from_pointer(section,7);
		smf_track_add_event_delta_pulses(track,event,offset * smf->ppqn);

	} else if (type == Chord) {

		event = smf_event_new_from_pointer(chord,9);
		smf_track_add_event_delta_pulses(track,event,offset * smf->ppqn);

	} else if (type == Tempo) {

		if (tempo_override == NULL) {

			event = smf_event_new_from_pointer(tempo,6);
			if (tpchn > 0) {
				smf_track_add_event_delta_pulses(track,event,offset * smf->ppqn);
			} else {
				/* Place the first tempo change definition at the very begining */
				smf_track_add_event_pulses(track,event,0);
				tpchn++;
			}
		}

	} else if (type == TSig) {

		event = smf_event_new_from_pointer(tsignature,7);
		smf_track_add_event_delta_pulses(track,event,offset * smf->ppqn);

	} else if (type == Stop) {

		/* End of track metaevent */
		if (smf_track_add_eot_delta_pulses(track,offset * smf->ppqn - 1))
			puts("Adding EOT failed.");

	}
}

static void add_midi_file_metronome()
{
	/* Generate midi metronome notes at each quater note
		in order for output file to be playable on Keyboard */
	
	for (uint32_t p=4*smf->ppqn; p < smf_length; p+=smf->ppqn) {
		event = smf_event_new_from_bytes(0x99,56,32);	//Channel 10, Cowbell, Velocity=32;
		smf_track_add_event_pulses(track,event,p);

		event = smf_event_new_from_bytes(0x89,56,0);	//Note Off
		smf_track_add_event_pulses(track,event,p+smf->ppqn/2);
	}
}

static void output_smf_event(snd_seq_tick_time_t tt, uint32_t *c)
{
	while (event->time_pulses == tt) {

		snd_seq_ev_clear(&ev);
		snd_seq_ev_set_source(&ev, 0);
		snd_seq_ev_set_subs(&ev);

		if (smf_event_is_sysex(event)) {

			snd_seq_ev_set_sysex(&ev, event->midi_buffer_length, event->midi_buffer);

			snd_seq_ev_schedule_tick(&ev, queue, 0, tt);
			snd_seq_event_output(seq, &ev);
			*c++;

		} else if (smf_event_is_metadata(event)) {

			if (event->midi_buffer[0] == 0xFF &&
				event->midi_buffer[1] == 0x51 &&
				event->midi_buffer[2] == 0x03) {

				/* Tempo meta event - output on the Timer as well */

				uint32_t tval = event->midi_buffer[5];
				tval |= event->midi_buffer[4] << 8;
				tval |= event->midi_buffer[3] << 16;

				snd_seq_ev_clear(&ev);
				snd_seq_ev_set_source(&ev, 0);
				snd_seq_ev_set_queue_tempo(&ev, queue, tval);
				snd_seq_ev_schedule_tick(&ev, queue, 0, tt);
				snd_seq_event_output(seq, &ev);
			}
		}

		event = smf_track_get_next_event(track);

	}
}

static void playback()
{
	/* Output data from SMF structure to ALSA sequencer*/
	init_seq();
	snd_seq_create_simple_port(seq,"Port 0", 
			SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,
			SND_SEQ_PORT_TYPE_MIDI_GENERIC);
	queue = snd_seq_alloc_named_queue(seq,"playback");

	/* Subscribe to selected destination port*/
	snd_seq_addr_t addr;
	snd_seq_parse_address(seq,&addr,addr_s);
	snd_seq_connect_to(seq, 0, addr.client, addr.port);

	/* Obtain memory pool size for timing setting */
	snd_seq_client_pool_t *cpool;
	snd_seq_client_pool_alloca(&cpool);
	snd_seq_get_client_pool(seq,cpool);

	/* Maximal number of events to be put on the queue in one attempt */
	const uint32_t mxevn = snd_seq_client_pool_get_output_pool(cpool)*2/5;

	/* Queue refill threshold */
	const uint32_t qrfth = snd_seq_client_pool_get_output_pool(cpool)/2;

	//Sequencer ticks per MIDI Clock
	const snd_seq_tick_time_t tpmc = smf->ppqn/24;

	//Set queue tempo equal to SMF
	snd_seq_queue_tempo_t *qtempo;
	snd_seq_queue_tempo_alloca(&qtempo);
	snd_seq_queue_tempo_set_ppq(qtempo,smf->ppqn);
	snd_seq_queue_tempo_set_tempo(qtempo,smf_tempo->microseconds_per_quarter_note);
	snd_seq_set_queue_tempo(seq,queue,qtempo);

	/* Schedule START to System:Timer*/
	snd_seq_ev_clear(&ev);
	snd_seq_ev_set_source(&ev, 0);
	snd_seq_ev_set_queue_start(&ev, queue);
	snd_seq_ev_schedule_tick(&ev, queue, 0, 0);
	snd_seq_event_output(seq, &ev);

	puts("Playback...");

	snd_seq_tick_time_t qtc = 0; //queue tick counter

	snd_seq_queue_status_t *qst;
	snd_seq_queue_status_alloca(&qst);

	uint32_t evn = 0; //event number
	snd_seq_tick_time_t c;
	const snd_seq_real_time_t *rt;

	do {
		if (evn < qrfth) {
			/* Queue is going to be empty */
			if (qtc <= smf_length) {
			/* We are not at the end */
				uint32_t gec = 0; //generated event counter
				while (gec < mxevn) {

					/* Events at 0 tick should be output before realtime Start */
					if (qtc == 0) {
						event = smf_track_get_next_event(track);
						output_smf_event(qtc, &gec);
					}

					/* Generate Real time event */
					snd_seq_ev_clear(&ev);
					snd_seq_ev_set_source(&ev, 0);
					snd_seq_ev_set_subs(&ev);

					if (qtc == 0)
						ev.type = SND_SEQ_EVENT_START;
					else if (qtc < smf_length)
						ev.type = SND_SEQ_EVENT_CLOCK;
					else
						ev.type = SND_SEQ_EVENT_STOP;

					snd_seq_ev_schedule_tick(&ev, queue, 0, qtc);
					snd_seq_event_output(seq, &ev);
					gec++;

					/* Send SYSEX if any on the actual counter value */
					if (qtc < smf_length)
						output_smf_event(qtc, &gec);

					qtc += tpmc;
					if (ev.type == SND_SEQ_EVENT_STOP) break;

				}

				snd_seq_drain_output(seq);

			}
		}

		usleep(100000);//100ms

		snd_seq_get_queue_status(seq,queue,qst);

		evn = snd_seq_queue_status_get_events(qst);
		c = snd_seq_queue_status_get_tick_time(qst);
		rt = snd_seq_queue_status_get_real_time(qst);

		snd_seq_get_queue_tempo(seq,queue,qtempo);

		printf("\rRT:%3u.%09u TT:%6u QE:%3u Tempo:%3.0f BPM",
				rt->tv_sec,rt->tv_nsec,c,evn,
				6e7/snd_seq_queue_tempo_get_tempo(qtempo));
		fflush(stdout);
	} while (evn > 0);

	puts("\nSTOP");

	snd_seq_sync_output_queue(seq);
	snd_seq_close(seq);
}

int
main(int argc, char *argv[])
{
	opterr = 0;
	char opt;
	char do_list = 0;

	while ((opt = getopt(argc, argv, "hi:lp:o:t:")) != -1) {
		switch (opt) {
			case 'h':
				show_usage();
				exit(EXIT_SUCCESS);
			case 'i':
				infilename = optarg;
				break;
			case 'l':
				do_list = 1;
				break;
			case 'o':
				outfilename = optarg;
				break;
			case 'p':
				addr_s = optarg;
				break;
			case 't':
				tempo_override = optarg;
				break;
			default:
				show_usage();
				exit(EXIT_FAILURE);
		}
	}

	if (do_list) {
		init_seq();
		list_ports();
		snd_seq_close(seq);
		exit(EXIT_SUCCESS);
	}

	if (argc < 5) {
			show_usage();
			exit(EXIT_FAILURE);
	}

	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);

	infile = fopen(infilename, "r");
	if (infile == NULL) {
		perror("Could not open input file.");
		exit(EXIT_FAILURE);
	}

	/* import data to SMF structure for further processing */
	init_smf();
	while (fgets(line,LINE_LENGTH,infile) != NULL) {
		if (process_line() > 1) 
			add_midi_file_event();
	}

	fclose(infile);

	smf_tempo = smf_get_last_tempo(smf);

	smf_length = smf_get_length_pulses(smf);

	/* Print loaded data summary */
	printf("PPQN             = %u\n", smf->ppqn);
	printf("Events number    = %u\n", track->number_of_events);
	printf("Length (pulses)  = %u\n", smf_length);
	printf("Length (seconds) = %3.3f\n", smf_get_length_seconds(smf));

	printf("Tempo            = %u us/qn\n", smf_tempo->microseconds_per_quarter_note);
	printf("Tempo            = %3.0f BPM\n", 6e7/smf_tempo->microseconds_per_quarter_note);
	printf("Time Signature   = %d/%d\n", smf_tempo->numerator,smf_tempo->denominator);
	printf("Clocks/Click     = %u\n", smf_tempo->clocks_per_click);
	printf("Notes/Note       = %u\n", smf_tempo->notes_per_note);

	if (outfilename == NULL) {
		/* Play SMF structure on sequencer output */
		smf_rewind(smf);
		playback();
	} else {
		/* Write SMF structure as file */
		if (smf_save(smf,outfilename)) perror("Could not save output MIDI file.");
	}

	smf_delete(smf);

	exit(EXIT_SUCCESS);
}
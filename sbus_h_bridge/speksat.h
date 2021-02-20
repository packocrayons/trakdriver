#include <stdint.h> 

#define NUM_SER_BYTES 16
#define NUM_CHANNELS 8
#define NUM_DATA_FRAMES 7
#define NON_CH_FRAMES 2

int get_sat_data(char bytes[]);
int process_channels(char serdata[], uint16_t channels[]);

#ifndef SENSOR_H
#define SENSOR_H

#include <SoftwareSerial.h>
#include <jled.h>
#include "DHT.h"
#include "debug.h"

// SML constants
const byte START_SEQUENCE[] = {0x1B, 0x1B, 0x1B, 0x1B, 0x01, 0x01, 0x01, 0x01};
const byte END_SEQUENCE[] = {0x1B, 0x1B, 0x1B, 0x1B, 0x1A};
const size_t BUFFER_SIZE = 3840; // Max datagram duration 400ms at 9600 Baud
const uint8_t READ_TIMEOUT = 30;
const uint8_t SAMPLING_RATE = 60;

// States
enum State
{
    INIT,
    WAIT_FOR_START_SEQUENCE,
    READ_MESSAGE,
    PROCESS_MESSAGE,
    READ_CHECKSUM
};

class SensorConfig
{
public:
    const uint8_t pin;
    const char *name;
    const char *type;
    const bool numeric_only;
    const bool status_led_enabled;
    const bool status_led_inverted;
    const uint8_t status_led_pin;
};

class Sensor
{
public:
    const SensorConfig *config;
    Sensor(const SensorConfig *config, void (*callback)(byte *buffer, size_t len,  Sensor *sensor))
    {
        this->config = config;
        DEBUG("Initializing sensor %s...", this->config->name);
        if ( strcmp (this->config->type,"SML") == 0) {
          DEBUG("Type: %s...", this->config->type);
          this->callback = callback;
          this->serial = new SoftwareSerial();
          this->serial->begin(9600, SWSERIAL_8N1, this->config->pin, -1, false);
          this->serial->enableTx(false);
          this->serial->enableRx(true);
        }
        if ( strcmp (this->config->type,"AM2302") == 0) {
          DEBUG("Type: %s...", this->config->type);
          this->callback = callback;
          this->dht = new DHT(this->config->pin, DHT22);
	        this->dht->begin();
        }
        DEBUG("Initialized sensor %s.", this->config->name);

        if (this->config->status_led_enabled) {
            this->status_led = new JLed(this->config->status_led_pin);
            if (this->config->status_led_inverted) {
                this->status_led->LowActive();
            }
        }

        this->init_state();
    }

    void loop()
    {
        this->run_current_state();
        yield();
        if (this->config->status_led_enabled) {
            this->status_led->Update();
            yield();
        }
    }

private:
    SoftwareSerial *serial;
    DHT *dht;
    byte buffer[BUFFER_SIZE];
    size_t position = 0;
    unsigned long last_state_reset = 0;
    unsigned long last_sample = 0;
    uint8_t bytes_until_checksum = 0;
    uint8_t loop_counter = 0;
    State state = INIT;
    void (*callback)(byte *buffer, size_t len, Sensor *sensor) = NULL;
    JLed *status_led;

    void run_current_state()
    {
        if (this->state != INIT)
        {
          if ( strcmp (this->config->type,"SML") == 0) {
            if ((millis() - this->last_state_reset) > (READ_TIMEOUT * 1000))
            {
                DEBUG("Did not receive an SML message within %d seconds, starting over.", READ_TIMEOUT);
                this->reset_state();
            }
          }
            switch (this->state)
            {
            case WAIT_FOR_START_SEQUENCE:
                this->wait_for_start_sequence();
                break;
            case READ_MESSAGE:
                this->read_message();
                break;
            case PROCESS_MESSAGE:
                this->process_message();
                break;
            case READ_CHECKSUM:
                this->read_checksum();
                break;
            default:
                break;
            }
        }
    }

    // Wrappers for sensor access
    int data_available()
    {
        return this->serial->available();
    }
    int data_read()
    {
        return this->serial->read();
    }


    // Set state
    void set_state(State new_state)
    {
        if (new_state == WAIT_FOR_START_SEQUENCE)
        {
            DEBUG("State of sensor %s type %s is 'WAIT_FOR_START_SEQUENCE'.", this->config->name, this->config->type);
            this->last_state_reset = millis();
            this->last_sample = millis();
            this->position = 0;
        }
        else if (new_state == READ_MESSAGE)
        {
            DEBUG("State of sensor %s type %s is 'READ_MESSAGE'.", this->config->name, this->config->type);
        }
        else if (new_state == READ_CHECKSUM)
        {
            DEBUG("State of sensor %s type %s is 'READ_CHECKSUM'.", this->config->name, this->config->type);
            this->bytes_until_checksum = 3;
        }
        else if (new_state == PROCESS_MESSAGE)
        {
            DEBUG("State of sensor %s type %s is 'PROCESS_MESSAGE'.", this->config->name, this->config->type);
        };
        this->state = new_state;
    }

    // Initialize state machine
    void init_state()
    {
        this->set_state(WAIT_FOR_START_SEQUENCE);
    }

    // Start over and wait for the start sequence
    void reset_state(const char *message = NULL)
    {
        if (message != NULL && strlen(message) > 0)
        {
            DEBUG(message);
        }
        this->init_state();
    }

    // Wait for the start_sequence to appear
    void wait_for_start_sequence()
    {
        if ( strcmp (this->config->type,"SML") == 0) {
            while (this->data_available())
            {
                this->buffer[this->position] = this->data_read();
                yield();

                this->position = (this->buffer[this->position] == START_SEQUENCE[this->position]) ? (this->position + 1) : 0;
                if (this->position == sizeof(START_SEQUENCE))
                {
                    // Start sequence has been found
                    DEBUG("Start sequence found.");
                    if (this->config->status_led_enabled) {
                        this->status_led->Blink(50,50).Repeat(3);
                    }
                    this->set_state(READ_MESSAGE);
                    return;
                }
            }
	      }
        if ( strcmp (this->config->type,"AM2302") == 0) {
            if ((millis() - this->last_sample) > (SAMPLING_RATE * 1000))
            {

                DEBUG("AM2302 Sample time %d reached", SAMPLING_RATE);
                // create float to byte array construct
                union memory{
                  byte asBytes[8];
                  float asFloat[2];
                };
                union memory tobuffer;
                // Read humidity
                tobuffer.asBytes[0] = this->dht->readHumidity();
                DEBUG("humidity: %d", tobuffer.asBytes[0]);
                // Read temperature as Celsius
                tobuffer.asBytes[1] = this->dht->readTemperature();
                DEBUG("temperature: %d", tobuffer.asBytes[1]);
                // Check if any reads failed and exit early (to try again).
                if (isnan(tobuffer.asBytes[0]) || isnan(tobuffer.asBytes[1]) ) {
                  DEBUG("Failed to read from DHT sensor!");
                  this->reset_state();
                }
                for (uint y = 0; y <= (sizeof(tobuffer) -1 ); y++)
                {
                  this->buffer[y] = tobuffer.asBytes[y];
                }
                this->set_state(READ_MESSAGE);
            }

	      }
    }

    // Read the rest of the message
    void read_message()
    {
        if ( strcmp (this->config->type,"SML") == 0) {
            while (this->data_available())
            {
                // Check whether the buffer is still big enough to hold the number of fill bytes (1 byte) and the checksum (2 bytes)
                if ((this->position + 3) == BUFFER_SIZE)
                {
                    this->reset_state("Buffer will overflow, starting over.");
                    return;
                }
                this->buffer[this->position++] = this->data_read();
                yield();

                // Check for end sequence
                int last_index_of_end_seq = sizeof(END_SEQUENCE) - 1;
                for (int i = 0; i <= last_index_of_end_seq; i++)
                {
                    if (END_SEQUENCE[last_index_of_end_seq - i] != this->buffer[this->position - (i + 1)])
                    {
                        break;
                    }
                    if (i == last_index_of_end_seq)
                    {
                        DEBUG("End sequence found.");
                        this->set_state(READ_CHECKSUM);
                        return;
                    }
                }
            }
	}
        if ( strcmp (this->config->type,"AM2302") == 0) {
	     // read
            this->set_state(PROCESS_MESSAGE);
	      }
    }

    // Read the number of fillbytes and the checksum
    void read_checksum()
    {
        while (this->bytes_until_checksum > 0 && this->data_available())
        {
            this->buffer[this->position++] = this->data_read();
            this->bytes_until_checksum--;
            yield();
        }

        if (this->bytes_until_checksum == 0)
        {
            DEBUG("Message has been read.");
            DEBUG_DUMP_BUFFER(this->buffer, this->position);
            this->set_state(PROCESS_MESSAGE);
        }
    }

    void process_message()
    {
        DEBUG("Message is being processed.");

        // Call listener
        if (this->callback != NULL)
        {
            this->callback(this->buffer, this->position, this);
        }
        // Start over
        this->reset_state();
    }
};

#endif

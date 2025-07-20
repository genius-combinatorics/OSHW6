#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define TOTAL_FARMERS 10
#define NORTH 0
#define SOUTH 1

// Synchronization variables
pthread_mutex_t direction_mutex;
sem_t bridge_sem;  // Single semaphore for direction control (initial value 0)
int current_direction = -1; // -1 means bridge is empty
int farmers_on_bridge = 0;
int waiting_north = 0;
int waiting_south = 0;

// Time tracking
time_t start_time;

const char* get_direction_name(int dir) {
    return dir == NORTH ? "NORTH" : "SOUTH";
}

void print_time() {
    time_t now = time(NULL) - start_time;
    printf("[%02ld:%02ld] ", now/60, now%60);
}

void* farmer(void* arg) {
    int* farmer_data = (int*)arg;
    int direction = farmer_data[0];
    int id = farmer_data[1];
    int cross_time = rand() % 3 + 1; // 1-3 seconds to cross
    
    print_time();
    printf("Farmer %d from %s ARRIVES at bridge\n", id, get_direction_name(direction));

    pthread_mutex_lock(&direction_mutex);
    
    // Check if we need to wait
    if (current_direction != -1 && current_direction != direction) {
        // Opposite direction - must wait
        if (direction == NORTH) waiting_north++;
        else waiting_south++;
        
        print_time();
        printf("Farmer %d from %s must WAIT - bridge is for %s direction\n", 
              id, get_direction_name(direction), get_direction_name(current_direction));
        
        pthread_mutex_unlock(&direction_mutex);
        
        // Wait on the single semaphore
        sem_wait(&bridge_sem);
        
        pthread_mutex_lock(&direction_mutex);
        if (direction == NORTH) waiting_north--;
        else waiting_south--;
    }
    
    // First farmer sets the direction
    if (current_direction == -1) {
        current_direction = direction;
        print_time();
        printf("Bridge direction set to %s by farmer %d\n", get_direction_name(direction), id);
    }
    
    farmers_on_bridge++;
    pthread_mutex_unlock(&direction_mutex);

    // Cross the bridge
    print_time();
    printf("Farmer %d from %s STARTS crossing (will take %d seconds)\n", 
          id, get_direction_name(direction), cross_time);
    sleep(cross_time);
    
    // Exit bridge
    pthread_mutex_lock(&direction_mutex);
    print_time();
    printf("Farmer %d from %s LEAVES bridge\n", id, get_direction_name(direction));
    farmers_on_bridge--;
    
    // Check if we should switch directions
    if (farmers_on_bridge == 0) {
        if ((current_direction == NORTH && waiting_south > 0) ||
            (current_direction == SOUTH && waiting_north > 0)) {
            int new_direction = !current_direction;
            int num_waiting = (new_direction == NORTH) ? waiting_north : waiting_south;
            
            current_direction = new_direction;
            print_time();
            printf("Bridge direction changed to %s (%d waiting)\n", 
                  get_direction_name(current_direction), num_waiting);
            
            // Release all waiting farmers of the new direction
            for (int i = 0; i < num_waiting; i++) {
                sem_post(&bridge_sem);
            }
        } else if (waiting_north == 0 && waiting_south == 0) {
            current_direction = -1;
            print_time();
            printf("Bridge is now EMPTY\n");
        }
    }
    pthread_mutex_unlock(&direction_mutex);

    free(arg);
    return NULL;
}

int main() {
    pthread_t farmers[TOTAL_FARMERS];
    start_time = time(NULL);
    
    pthread_mutex_init(&direction_mutex, NULL);
    sem_init(&bridge_sem, 0, 0);  // Initial value 0 - blocked
    srand(time(NULL));

    printf("Starting bridge simulation with %d farmers...\n", TOTAL_FARMERS);

    // Create farmers with random arrival times and directions
    for (int i = 0; i < TOTAL_FARMERS; i++) {
        int* farmer_data = malloc(2 * sizeof(int));
        farmer_data[0] = rand() % 2; // Random direction
        farmer_data[1] = i + 1;      // Farmer ID
        
        pthread_create(&farmers[i], NULL, farmer, farmer_data);
        usleep(rand() % 3000000); // Random arrival delay (0-3 second)
    }

    for (int i = 0; i < TOTAL_FARMERS; i++) {
        pthread_join(farmers[i], NULL);
    }

    pthread_mutex_destroy(&direction_mutex);
    sem_destroy(&bridge_sem);
    
    printf("Simulation complete after %ld seconds\n", time(NULL) - start_time);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <pthread.h>  
#include <sys/stat.h> 
#include <stdatomic.h> 

#define PORT 8080
#define BUFFER_SIZE 8192
#define QUEUE_SIZE 100       

atomic_int global_submission_id = 0;

int client_queue[QUEUE_SIZE];
int queue_front = 0;
int queue_rear = 0;
int queue_count = 0;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

void enqueue(int client_socket) {
    pthread_mutex_lock(&queue_mutex);
    if (queue_count < QUEUE_SIZE) {
        client_queue[queue_rear] = client_socket;
        queue_rear = (queue_rear + 1) % QUEUE_SIZE;
        queue_count++;
        pthread_cond_signal(&queue_cond); 
    } else {
        close(client_socket); 
    }
    pthread_mutex_unlock(&queue_mutex);
}

int dequeue() {
    pthread_mutex_lock(&queue_mutex);
    while (queue_count == 0) pthread_cond_wait(&queue_cond, &queue_mutex);
    int client_socket = client_queue[queue_front];
    queue_front = (queue_front + 1) % QUEUE_SIZE;
    queue_count--;
    pthread_mutex_unlock(&queue_mutex);
    return client_socket;
}

bool compileCode(const char* workspace) {
    pid_t pid = fork();
    if (pid == 0) {
        char src_path[256], out_path[256];
        sprintf(src_path, "%s/solution.cpp", workspace);
        sprintf(out_path, "%s/solution.out", workspace);
        execlp("g++", "g++", "-O2", "-static", src_path, "-o", out_path, NULL);
        exit(1);
    } else {
        int status;
        waitpid(pid, &status, 0);
        return (WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
}

int runCode(int test_num, const char* workspace, int problem_id, long *out_time_ms, long *out_mem_kb) {
    char in_filename[512];
    sprintf(in_filename, "tests/prob_%d/in%d.txt", problem_id, test_num);
    
    int in_fd = open(in_filename, O_RDONLY);
    if (in_fd < 0) return 404; 

    pid_t pid = fork();
    if (pid == 0) {
        struct rlimit cpu_limit = {1, 2}; 
        setrlimit(RLIMIT_CPU, &cpu_limit);
        struct rlimit mem_limit = {1024 * 1024 * 1024, 1024 * 1024 * 1024}; 
        setrlimit(RLIMIT_AS, &mem_limit);
        struct rlimit stack_limit = {64 * 1024 * 1024, 64 * 1024 * 1024}; 
        setrlimit(RLIMIT_STACK, &stack_limit);

        char out_filename[256];
        sprintf(out_filename, "%s/my_output.txt", workspace); 

        int out_fd = open(out_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        
        dup2(in_fd, 0); 
        dup2(out_fd, 1); 
        close(in_fd);
        close(out_fd);

        if (chdir(workspace) != 0) exit(200); 
        if (chroot(".") != 0) exit(200);

        execlp("/solution.out", "solution.out", NULL);
        exit(1);
    } else {
        close(in_fd); 
        
        int status;
        struct rusage usage;
        
        wait4(pid, &status, 0, &usage);

        *out_time_ms = (usage.ru_utime.tv_sec * 1000 + usage.ru_utime.tv_usec / 1000) +
                       (usage.ru_stime.tv_sec * 1000 + usage.ru_stime.tv_usec / 1000);
        *out_mem_kb = usage.ru_maxrss;

        if (WIFEXITED(status) && WEXITSTATUS(status) == 200) return 2; 
        
        // --- FEATURE 3: Advanced Signal Trapping ---
        if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);
            if (sig == SIGXCPU) return 1; // Time Limit Exceeded
            if (sig == SIGKILL && *out_time_ms >= 1000) return 1; 
            if (*out_mem_kb >= 950000 || (sig == SIGSEGV && *out_mem_kb >= 900000)) return 3; // Memory Limit Exceeded
            return 2; // Generic Runtime Error
        }
        
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) return 2; 
        return 0; 
    }
}

bool checkAnswer(int test_num, const char* workspace, int problem_id) {
    char expected_filename[512], my_out_filename[512];
    
    sprintf(expected_filename, "tests/prob_%d/out%d.txt", problem_id, test_num);
    sprintf(my_out_filename, "%s/my_output.txt", workspace);
    
    FILE *my_out = fopen(my_out_filename, "r");
    FILE *expected_out = fopen(expected_filename, "r");
    if (!my_out || !expected_out) {
        if (my_out) fclose(my_out);
        if (expected_out) fclose(expected_out);
        return false;
    }

    char my_word[1024], exp_word[1024];
    bool is_correct = true;
    while (fscanf(expected_out, "%1023s", exp_word) == 1) {
        if (fscanf(my_out, "%1023s", my_word) != 1 || strcmp(my_word, exp_word) != 0) {
            is_correct = false;
            break;
        }
    }
    if (is_correct && fscanf(my_out, "%1023s", my_word) == 1) is_correct = false;
    fclose(my_out);
    fclose(expected_out);
    return is_correct;
}

void grade_submission(const char* workspace, int problem_id, char* final_result) {
    if (!compileCode(workspace)) {
        strcpy(final_result, "Result: Compilation Error (CE)!\n");
        return;
    }

    int test_num = 1;
    int passed = 0;
    int total_run = 0;
    strcpy(final_result, "--- Autograder Results ---\n");

    while (true) {
        long time_ms = 0;
        long mem_kb = 0;
        int exec_status = runCode(test_num, workspace, problem_id, &time_ms, &mem_kb);
        
        if (exec_status == 404) break; 

        total_run++;
        char temp_buffer[128];

        if (exec_status == 1) {
            sprintf(temp_buffer, "Test %d: Time Limit Exceeded (TLE)\n", test_num);
        } else if (exec_status == 2) {
            sprintf(temp_buffer, "Test %d: Runtime Error (RE)\n", test_num);
        } else if (exec_status == 3) {
            sprintf(temp_buffer, "Test %d: Memory Limit Exceeded (MLE)\n", test_num);
        } else if (exec_status == 0) {
            if (checkAnswer(test_num, workspace, problem_id)) {
                sprintf(temp_buffer, "Test %d: Accepted (AC) [%ld ms, %ld KB]\n", test_num, time_ms, mem_kb);
                passed++;
            } else {
                sprintf(temp_buffer, "Test %d: Wrong Answer (WA) [%ld ms, %ld KB]\n", test_num, time_ms, mem_kb);
            }
        }
        strcat(final_result, temp_buffer);
        test_num++;
    }

    if (total_run > 0) {
        char score_buffer[100];
        sprintf(score_buffer, "\nFinal Score: Passed %d out of %d tests.\n", passed, total_run);
        strcat(final_result, score_buffer);
    } else {
        strcat(final_result, "\nError: Invalid Problem ID or missing test cases.\n");
    }
}

void* worker_logic(void* arg) {
    int thread_id = *(int*)arg;
    
    while (true) {
        int client_socket = dequeue();
        char workspace[256];
        
        int current_job = atomic_fetch_add(&global_submission_id, 1);
        sprintf(workspace, "workspace_%d_job_%d", thread_id, current_job);
        mkdir(workspace, 0777); 
        
        char buffer[8192] = {0};
        read(client_socket, buffer, sizeof(buffer) - 1);
        
        char final_result[4096] = {0};

        // --- FEATURE 2: Dynamic Test Loading Payload Parser ---
        if (strncmp(buffer, "PROBLEM___", 10) == 0) {
            char *id_str = buffer + 10;
            char *split_ptr = strstr(id_str, "___SPLIT___");

            if (split_ptr) {
                *split_ptr = '\0';
                int problem_id = atoi(id_str);
                char *code = split_ptr + 11;

                char path[512];
                sprintf(path, "%s/solution.cpp", workspace);
                FILE *fc = fopen(path, "w");
                if (fc) {
                    fprintf(fc, "%s", code);
                    fclose(fc);
                }

                grade_submission(workspace, problem_id, final_result);
            } else {
                strcpy(final_result, "Error: Invalid Payload Format.\n");
            }
        } else {
            strcpy(final_result, "Error: Payload must start with PROBLEM___<ID>___SPLIT___\n");
        }

        send(client_socket, final_result, strlen(final_result), 0);
        close(client_socket);

        char rm_cmd[512];
        sprintf(rm_cmd, "rm -rf %s", workspace);
        system(rm_cmd);
    }
    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    int optimal_thread_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (optimal_thread_count < 1) optimal_thread_count = 1; 

    pthread_t *thread_pool = malloc(optimal_thread_count * sizeof(pthread_t));
    int *thread_ids = malloc(optimal_thread_count * sizeof(int));
    
    for (int i = 0; i < optimal_thread_count; i++) {
        thread_ids[i] = i;
        pthread_create(&thread_pool[i], NULL, worker_logic, &thread_ids[i]);
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) exit(EXIT_FAILURE);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) exit(EXIT_FAILURE);
    if (listen(server_fd, 100) < 0) exit(EXIT_FAILURE);

    while (true) {
        int client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_socket > 0) enqueue(client_socket);
    }
    return 0;
}
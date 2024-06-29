#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

// Estrutura para uma entrada da tabela de páginas
typedef struct {
    unsigned int page_number;
    int frame_number;
    int modified;
    int referenced;
    unsigned long last_access;
} PageTableEntry;

// Estrutura para um quadro de memória
typedef struct {
    unsigned int frame_number;
    unsigned int page_number;
    int modified;
    int referenced;        // Adicionado para algoritmos como Segunda Chance
    unsigned long last_access;  // Adicionado para LRU
} Frame;

// Função para calcular o valor de 's'
unsigned calculate_s(unsigned page_size) {
    unsigned s = 0;
    while (page_size > 1) {
        page_size >>= 1;
        s++;
    }
    return s;
}

// Algoritmo de substituição FIFO
int fifo_replace(Frame *frames, int num_frames) {
    static int next_frame = 0;
    int frame_to_replace = next_frame;
    next_frame = (next_frame + 1) % num_frames;
    return frame_to_replace;
}

// Algoritmo de substituição LRU
int lru_replace(Frame *frames, int num_frames) {
    unsigned long min_time = ULONG_MAX;
    int frame_to_replace = 0;

    for (int i = 0; i < num_frames; i++) {
        if (frames[i].last_access < min_time) {
            min_time = frames[i].last_access;
            frame_to_replace = i;
        }
    }

    return frame_to_replace;
}

// Algoritmo de substituição Segunda Chance
int second_chance_replace(Frame *frames, int num_frames) {
    static int pointer = 0;
    
    while (1) {
        if (!frames[pointer].referenced) {
            int frame_to_replace = pointer;
            pointer = (pointer + 1) % num_frames;
            return frame_to_replace;
        }
        frames[pointer].referenced = 0;
        pointer = (pointer + 1) % num_frames;
    }
}

// Algoritmo de substituição aleatória
int random_replace(int num_frames) {
    return random() % num_frames;
}

// Função para simular o acesso à memória
void simulate_memory_access(FILE *file, int page_size, int mem_size, const char *replacement_algorithm) {
    int num_frames = mem_size / page_size;
    Frame *frames = calloc(num_frames, sizeof(Frame));
    if (!frames) {
        fprintf(stderr, "Erro ao alocar memória para os quadros.\n");
        exit(1);
    }

    unsigned int num_pages = 1 << (32 - calculate_s(page_size));  // Calcula o tamanho da tabela de páginas baseado em page_size
    PageTableEntry *page_table = calloc(num_pages, sizeof(PageTableEntry));
    if (!page_table) {
        fprintf(stderr, "Erro ao alocar memória para a tabela de páginas.\n");
        free(frames);
        exit(1);
    }

    int page_faults = 0;
    int pages_written = 0;
    unsigned long access_count = 0;

    unsigned addr;
    char rw_char;
    while (fscanf(file, "%x %c", &addr, &rw_char) != EOF) {
        access_count++;
        unsigned page = addr >> calculate_s(page_size);
        int found = 0;

        // Verificação na tabela de páginas
        for (int j = 0; j < num_frames; j++) {
            if (frames[j].page_number == page) {
                frames[j].last_access = access_count;
                frames[j].referenced = 1;
                if (rw_char == 'W') {
                    frames[j].modified = 1;
                }
                found = 1;
                break;
            }
        }

        // Se não encontrada, ocorre page fault
        if (!found) {
            page_faults++;
            int frame_to_replace;
            
            if (strcmp(replacement_algorithm, "fifo") == 0) {
                frame_to_replace = fifo_replace(frames, num_frames);
            } else if (strcmp(replacement_algorithm, "lru") == 0) {
                frame_to_replace = lru_replace(frames, num_frames);
            } else if (strcmp(replacement_algorithm, "2a") == 0) {
                frame_to_replace = second_chance_replace(frames, num_frames);
            } else { // random
                frame_to_replace = random_replace(num_frames);
            }

            // Se o frame estava modificado, deve ser escrito de volta
            if (frames[frame_to_replace].modified) {
                pages_written++;
            }

            // Carregar a nova página
            frames[frame_to_replace].page_number = page;
            frames[frame_to_replace].last_access = access_count;
            frames[frame_to_replace].modified = (rw_char == 'W');
            frames[frame_to_replace].referenced = 1;
        }
    }

    // Relatório final
    printf("Tamanho da memória: %d KB\n", mem_size);
    printf("Tamanho das páginas: %d KB\n", page_size);
    printf("Técnica de reposição: %s\n", replacement_algorithm);
    printf("Número de acessos à memória: %lu\n", access_count);
    printf("Número de page faults: %d\n", page_faults);
    printf("Número de páginas escritas: %d\n", pages_written);

    free(frames);
    free(page_table);
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Uso: %s algoritmo arquivo.log tamanho_pagina tamanho_memoria\n", argv[0]);
        return 1;
    }

    const char *replacement_algorithm = argv[1];
    const char *log_file = argv[2];
    unsigned page_size = atoi(argv[3]);
    unsigned mem_size = atoi(argv[4]);

    FILE *file = fopen(log_file, "r");
    if (!file) {
        fprintf(stderr, "Erro ao abrir o arquivo: %s\n", log_file);
        return 1;
    }

    simulate_memory_access(file, page_size, mem_size, replacement_algorithm);
    fclose(file);

    return 0;
}

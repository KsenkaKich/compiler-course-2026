// RUN: %clang_cc1 -load %llvmshlibdir/kichanova_k_FIIT3_lab1_ClangAST%pluginext -plugin analyzer_kichanova_plugin -fsyntax-only %s 2>&1 | FileCheck %s

extern "C" {
    void* malloc(unsigned long size);
    void* calloc(unsigned long count, unsigned long size);
    void* realloc(void* ptr, unsigned long size);
    void free(void* ptr);
    void* fopen(const char* filename, const char* mode);
    int fclose(void* stream);
}

// тесты утечек ресурсов

// CHECK-DAG: warning: resource leak at line [[#]] - memory
int* leak1 = (int*)malloc(100);

// CHECK-DAG: warning: resource leak at line [[#]] - memory
int* leak2 = (int*)calloc(10, sizeof(int));

// CHECK-DAG: warning: resource leak at line [[#]] - memory
int* leak3 = (int*)realloc(nullptr, 200);

// CHECK-DAG: warning: resource leak at line [[#]] - file
void* file = fopen("test.txt", "r");

// CHECK-DAG: warning: resource leak at line [[#]] - memory
int* new_leak1 = new int(42);

// CHECK-DAG: warning: resource leak at line [[#]] - memory
int* new_leak2 = new int[100];

void multiple_leaks() {
    // CHECK-DAG: warning: resource leak at line [[#]] - memory
    int* a = (int*)malloc(10);

     // CHECK-DAG: warning: resource leak at line [[#]] - memory
    int* b = new int(5);

    // CHECK-DAG: warning: resource leak at line [[#]] - file
    void* f = fopen("test.txt", "r");
}
void if_leak(int x) {
    if (x > 0) {
        // CHECK-DAG: warning: resource leak at line [[#]] - memory
        int* p = (int*)malloc(10);
    }    
}

// тесты без утечек


// CHECK-NOT: warning: resource leak at line [[#]] - memory
void correct_malloc() {
    int* p = (int*)malloc(100);
    free(p);
}

// CHECK-NOT: warning: resource leak at line [[#]] - memory
void correct_calloc() {
    int* p = (int*)calloc(10, sizeof(int));
    free(p);
}

// CHECK-NOT: warning: resource leak at line [[#]] - file
void correct_fopen() {
    void* f = fopen("test.txt", "r");
    fclose(f);
}

// CHECK-NOT: warning: resource leak at line [[#]] - memory
void correct_new1() {
    int* p = new int(42);
    delete p;
}

// CHECK-NOT: warning: resource leak at line [[#]] - memory
void correct_new2() {
    int* p = new int[100];
    delete[] p;
}

// CHECK-NOT: warning: resource leak at line [[#]] - memory
void correct_multiple() {
    int* a = (int*)malloc(10);
    int* b = new int(5);
    void* f = fopen("test.txt", "r"); 
    free(a);
    delete b;
    fclose(f);
}

// CHECK-NOT: warning: resource leak at line [[#]] - memory
void correct_if(int x) {
    int* p = (int*)malloc(10);
    if (x > 0) {
        free(p);
    } else {
        free(p);
    }
}

// CHECK-NOT: warning: resource leak at line [[#]] - memory
int* correct_return(int x) {
    int* p = new int(x);
    return p;
}


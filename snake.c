#include "ripes_system.h"
#include <stdlib.h>

/*─── SWITCH 0 ──────────────────────────────────────────────────────────────*/
#define SW0 (0x01)

/*─── DELAY ─────────────────────────────────────────────────────────────────*/
#define LOOPS_PER_MS  1000

/*─── CONFIGURACIÓN DE COLORES ──────────────────────────────────────────────*/
#define APPLE_COLOR 0x00e100
#define BLACK       0x000000
#define SNAKE_COLOR 0xff0000
#define ORANGE_COLOR 0xFF8000

/*─── ESTRUCTURAS Y ENUMERACIONES ───────────────────────────────────────────*/

// Nodo que representa un segmento (bloque 2×2 LEDs) de la serpiente
typedef struct node {
    volatile unsigned int** leds;
    struct node* next;
} Node;

// Serpiente con punteros a cabeza y cola para facilitar movimientos
typedef struct snake {
    Node* head;
    Node* tail;
    int   length;
} SnakeType;

// Manzana representada como un bloque 2×2 LEDs
typedef struct apple {
    volatile unsigned int** sections;
} AppleType;

// Posibles direcciones de movimiento
typedef enum { RIGHT, LEFT, UP, DOWN } motion;

// Tipos de colisiones detectables
typedef enum { COLLISION_NONE, COLLISION_SELF, COLLISION_APPLE } CollisionType;


/*─── FUNCIONES: MANZANA ──────────────────────────────────────────────────────*/

                // Inicializa la estructura de la manzana
AppleType*      initializeApple(volatile unsigned int* ledBase, int width);
                // Genera una nueva posición aleatoria para la manzana
int             generateApplePosition(AppleType* apple, volatile unsigned int* ledBase, int width, int height, int seed);
                // Actualiza posición y LEDs de la manzana tras ser comida
void            updateApple(AppleType* apple, volatile unsigned int* ledBase, int width, int height, int* seed);

/*─── FUNCIONES: SNAKE ────────────────────────────────────────────────────────*/

                // Inicializa la serpiente en la posición inicial (0,0)
SnakeType*      initializeSnake(volatile unsigned int* ledBase, int width);
                // Mueve la serpiente reutilizando el último nodo (cola) como nueva cabeza
void            motionSnake(SnakeType* snake, motion currentDir, int width);
                // Crece la serpiente creando un nuevo nodo frontal
void            growSnake(SnakeType* snake, motion currentDir, int width);
                // Detecta colisiones según el color de los LEDs frontales de la cabeza
CollisionType   checkCollisionByColor(volatile unsigned int* headLeds[2]);

/*─── FUNCIONES: UTILIDADES ────────────────────────────────────────────────────*/
            // Genera una posición aleatoria en la matriz LED
int         randomPosition(int width, int height, int seed);
            // Pinta un conjunto de LEDs con el color indicado
void        paintLEDs(volatile unsigned int* leds[], int count, unsigned int color);
            // Limpia toda la pantalla (matriz LED)
void        limpiarPantalla(volatile unsigned int* ledBase, int width, int height);
            // Verifica si un bloque 2×2 LEDs está libre (sin color)
int         isFreeZone(volatile unsigned int* ledBase, int pos, int width);


int         checkBoundary(int headX, int headY, int width, int height);
            // Crea un nodo nuevo dado una base LED (superior izquierda)
Node*       createNode(volatile unsigned int* blockBase, int width);
            // Espera un retardo en milisegundos
void        delay_ms(int ms);
            // Libera la memoria dinámica de la serpiente
void        freeSnake(SnakeType* snake);
// Calcula nueva base frontal de la serpiente según dirección
volatile unsigned int* computeNewHeadBase(volatile unsigned int* oldHeadLed, motion currentDir, int width);


/*─── FUNCIÓN PRINCIPAL ─────────────────────────────────────────────────────*/
/**
 * main:
 *   Punto de entrada del juego. Se encarga de inicializar la matriz LED,
 *   el D-Pad, la manzana y la serpiente, y contiene dos bucles:
 *   - Uno exterior que permite reiniciar la partida al pulsar switch 0.
 *   - Uno interior que ejecuta el juego hasta GAME OVER.
 */
int main() {
    // 1) Configurar punteros a la matriz LED y sus dimensiones
    volatile unsigned int* ledBase = LED_MATRIX_0_BASE;
    int width  = LED_MATRIX_0_WIDTH;
    int height = LED_MATRIX_0_HEIGHT;

    // 2) Configurar punteros al D-Pad y al switch 0
    volatile unsigned int * d_pad_up = D_PAD_0_UP;
    volatile unsigned int * d_pad_do = D_PAD_0_DOWN;
    volatile unsigned int * d_pad_le = D_PAD_0_LEFT;
    volatile unsigned int * d_pad_ri = D_PAD_0_RIGHT;
    volatile unsigned int * switch_base = SWITCHES_0_BASE;

    // Bucle exterior: reinicia la partida cada vez que se pulsa switch0
    while (1) {
        // 3) Limpiar toda la pantalla antes de cada partida
        limpiarPantalla(ledBase, width, height);

        // 4) Inicializar los objetos del juego
        AppleType*  apple = initializeApple(ledBase, width);
        SnakeType*  snake = initializeSnake(ledBase, width);

         // 5) Colocar la primera manzana en posición aleatoria
        int pos = 60;  // semilla inicial
        pos = generateApplePosition(apple, ledBase, width, height, pos);
        paintLEDs(apple->sections, 4, APPLE_COLOR);

        // 6) Definir la dirección inicial de la serpiente
        motion currentDir = DOWN;

        // Variables auxiliares para coordenadas de cabeza
        int index, headX, headY;
        volatile unsigned int* oldHeadLed;

        // Bucle interior de juego: se repetirá hasta GAME OVER
        while (1) {
            // 7.1) Leer D-Pad y actualizar currentDir (no permite 180°)
            if      (*d_pad_up == 1    && currentDir != DOWN)  currentDir = UP;
            else if (*d_pad_do == 1    && currentDir != UP)    currentDir = DOWN;
            else if (*d_pad_le == 1    && currentDir != RIGHT) currentDir = LEFT;
            else if (*d_pad_ri == 1    && currentDir != LEFT)  currentDir = RIGHT;

            // 7.2) Obtener la posición actual de la cabeza en coordenadas (x,y)
            oldHeadLed = snake->head->leds[0];
            index      = oldHeadLed - ledBase;   // índice lineal
            headX      = index %  width;         // columna
            headY      = index /  width;         // fila

            // 7.3) Simular el siguiente paso en (newX,newY)
            int newX = headX, newY = headY;
            switch (currentDir) {
                case UP:    newY--; break;
                case DOWN:  newY++; break;
                case LEFT:  newX--; break;
                case RIGHT: newX+=2; break;
            }

            // 7.4) Detectar colisión con el borde antes de mover
            if (checkBoundary(newX, newY, width, height)) {
                break;  // GAME OVER por salirse de la matriz
            }

            // 7.5) Detectar choque inminente leyendo solo 2 LEDs frontales
            volatile unsigned int* headFront[2];
            switch (currentDir) {
                case UP:
                    headFront[0] = oldHeadLed - width;
                    headFront[1] = oldHeadLed - width + 1;
                    break;
                case DOWN:
                    headFront[0] = oldHeadLed + 2*width;
                    headFront[1] = oldHeadLed + 2*width + 1;
                    break;
                case LEFT:
                    headFront[0] = oldHeadLed - 1;
                    headFront[1] = oldHeadLed - 1 + width;
                    break;
                default:  // RIGHT
                    headFront[0] = oldHeadLed + 2;
                    headFront[1] = oldHeadLed + 2 + width;
                    break;
            }

            // 7.6) Evaluar tipo de colisión y actuar en consecuencia
            CollisionType col = checkCollisionByColor(headFront);
            if (col == COLLISION_SELF) {
                break;  // GAME OVER al chocar contra sí misma
            }
            else if (col == COLLISION_APPLE) {
                // Crece y reposiciona la manzana
                updateApple(apple, ledBase, width, height, &pos);
                growSnake(snake, currentDir, width);
            }
            else {
                // Movimiento normal (sin crecer)
                motionSnake(snake, currentDir, width);
            }

            // 7.7) Retardo para controlar la velocidad del juego
            delay_ms(1);
        }

        // 8) Liberar memoria de la partida terminada
        free(apple->sections);
        free(apple);
        freeSnake(snake);

        
        // 9) Parpadeo de LED esquina en naranja esperando SW0
        volatile unsigned int* corner_led = ledBase;  // puntero a LED [0,0]
        while (!(*switch_base & SW0)) {
            // Enciende naranja
            *corner_led = ORANGE_COLOR;
            delay_ms(2);
            // Apaga
            *corner_led = BLACK;
            delay_ms(2);
        }
        // Al pulsar SW0, sale del bucle y reinicia la partida
    }

    return 0;
}


/*─── IMPLEMENTACIONES: APPLE ────────────────────────────────────────────────*/

/**
 * initializeApple:
 *   Reserva una estructura AppleType y un array de 4 punteros para
 *   las secciones de la manzana. No coloca aún la manzana en pantalla.
 */
AppleType* initializeApple(volatile unsigned int* ledBase, int width) {
    AppleType* a = malloc(sizeof(*a));
    a->sections  = malloc(4 * sizeof(*(a->sections)));
    return a;
}

/**
 * generateApplePosition:
 *   Usa randomPosition para obtener un índice lineal válido para un
 *   bloque 2×2. Asigna los cuatro punteros de apple->sections
 *   apuntando a esa posición (esquina sup-izq y sus tres LEDs vecinos).
 *   Devuelve el índice lineal para usar como semilla la próxima vez.
 */
int generateApplePosition(AppleType* apple, volatile unsigned int* ledBase, int width, int height, int seed) {
    int pos = randomPosition(width, height, seed);
    apple->sections[0] = ledBase + pos;
    apple->sections[1] = ledBase + pos + 1;
    apple->sections[2] = ledBase + pos + width;
    apple->sections[3] = ledBase + pos + width + 1;
    return pos;
}

/**
 * updateApple:
 *   Borra la manzana actual (poniendo BLACK en oldLEDs),
 *   genera una nueva posición aleatoria repetidamente hasta
 *   encontrar una zona libre (isFreeZone), y pinta la nueva
 *   manzana con APPLE_COLOR.
 */
void updateApple(AppleType* apple, volatile unsigned int* ledBase, int width, int height, int* seed) {
    // Guardar punteros antiguos para borrarlos
    volatile unsigned int* oldLEDs[4] = {
        apple->sections[0],
        apple->sections[1],
        apple->sections[2],
        apple->sections[3]
    };

    // Reubicar hasta hallar zona libre
    do {
        *seed = generateApplePosition(apple, ledBase, width, height, *seed);
    } while (!isFreeZone(ledBase, *seed, width));

    // Borrar colores de la vieja manzana
    paintLEDs(oldLEDs, 4, BLACK);
    // Pintar la nueva manzana
    paintLEDs(apple->sections, 4, APPLE_COLOR);
}



/*─── IMPLEMENTACIONES: SNAKE ────────────────────────────────────────────────*/

/**
 * initializeSnake:
 *   Reserva y retorna una estructura SnakeType con un solo bloque 2×2
 *   en la esquina superior-izquierda. Tanto head como tail apuntan
 *   a ese primer nodo, y se pinta con SNAKE_COLOR.
 */
SnakeType* initializeSnake(volatile unsigned int* ledBase, int width) {
    SnakeType* s = (SnakeType*)malloc(sizeof(SnakeType));
    Node* initial = createNode(ledBase, width);
    s->head   = initial;
    s->tail   = initial;
    s->length = 1;
    paintLEDs(initial->leds, 4, SNAKE_COLOR);
    return s;
}

/**
 * motionSnake:
 *   Avanza la serpiente un bloque en currentDir sin crecer.
 *   Apaga la cola, recicla ese nodo como nueva cabeza,
 *   actualiza sus LEDs al nuevo bloque y los enciende.
 */
void motionSnake(SnakeType* snake, motion currentDir, int width) {
    volatile unsigned int* oldHead = snake->head->leds[0];
    volatile unsigned int* newBase = computeNewHeadBase(oldHead, currentDir, width);

    volatile unsigned int* newLeds[4] = {
        newBase,
        newBase + 1,
        newBase + width,
        newBase + width + 1
    };

    if (snake->length == 1) {
        // Caso especial: solo hay un nodo, reubica sus LEDs directamente
        paintLEDs(snake->head->leds, 4, BLACK);
        for (int i = 0; i < 4; i++)
            snake->head->leds[i] = newLeds[i];
        paintLEDs(snake->head->leds, 4, SNAKE_COLOR);
        return;
    }

    // CASO GENERAL: longitud >= 2
    // 1) Apagar LEDs de la cola
    paintLEDs(snake->tail->leds, 4, BLACK);

    // 2) Guardar el nodo tail para reciclar
    Node* recycled = snake->tail;

    // 3) Mover el tail al siguiente
    snake->tail = recycled->next;

    // 4) Desconectar el nodo reciclado
    recycled->next = NULL;

    // 5) Actualizar los punteros de LEDs
    for (int i = 0; i < 4; i++)
        recycled->leds[i] = newLeds[i];

    // 6) Insertar el nodo reciclado como nuevo head
    recycled->next = NULL;
    snake->head->next = recycled;
    snake->head = recycled;

    // 7) Pintar los LEDs del nuevo bloque cabeza
    paintLEDs(recycled->leds, 4, SNAKE_COLOR);
}



/**
 * growSnake:
 *   Crea un nuevo nodo 2×2 en front de la cabeza sin tocar la cola,
 *   enlaza ese nodo como nueva cabeza y enciende sus LEDs.
 */
void growSnake(SnakeType* snake, motion currentDir, int width) {
    volatile unsigned int* oldHead = snake->head->leds[0];
    volatile unsigned int* newBase = computeNewHeadBase(oldHead, currentDir, width);

    Node* newNode = createNode(newBase, width);
    snake->head->next = newNode;
    snake->head   = newNode;
    snake->length++;
    paintLEDs(newNode->leds, 4, SNAKE_COLOR);
}

/**
 * checkCollisionByColor:
 *   Lee los dos LEDs frontales apuntados en headLeds.
 *   Devuelve COLLISION_SELF si coincide con SNAKE_COLOR,
 *   COLLISION_APPLE si coincide con APPLE_COLOR, o NONE si ambos son BLACK.
 */
CollisionType checkCollisionByColor(volatile unsigned int* headLeds[2]) {
    unsigned int c1 = *headLeds[0];
    unsigned int c2 = *headLeds[1];
    if (c1 == SNAKE_COLOR || c2 == SNAKE_COLOR) return COLLISION_SELF;
    if (c1 == APPLE_COLOR || c2 == APPLE_COLOR) return COLLISION_APPLE;
    return COLLISION_NONE;
}


/*─── IMPLEMENTACIONES: UTILIDADES ──────────────────────────────────────────*/

/**
 * randomPosition:
 *   - Reinicia el generador de números aleatorios usando `seed`.
 *   - Calcula una coordenada X aleatoria entre [0, width-2]
 *     y una Y aleatoria entre [0, height-2], para que quepa
 *     un bloque 2×2 dentro de los límites.
 *   - Devuelve la posición lineal = y*width + x, que corresponde
 *     a la esquina superior izquierda de ese bloque 2×2.
 */
int randomPosition(int width, int height, int seed) {
    srand(seed);
    int x = rand() % (width  - 1);
    int y = rand() % (height - 1);
    return y * width + x;
}

/**
 * paintLEDs:
 *   - Recibe un array de punteros a LEDs y un color.
 *   - Itera desde i=0 hasta i<count-1 y asigna `color`
 *     al LED apuntado por cada puntero.
 *   - Se usa tanto para encender segmentos de la serpiente
 *     (SNAKE_COLOR) como para borrar (BLACK).
 */
void paintLEDs(volatile unsigned int* leds[], int count, unsigned int color) {
    for (int i = 0; i < count; i++)
        *leds[i] = color;
}

/**
 * limpiarPantalla:
 *   - Recorre toda la matriz LED de tamaño width×height.
 *   - Asigna el color BLACK (apagado) a cada celda.
 *   - Se invoca al inicio del juego para empezar con pantalla limpia.
 */
void limpiarPantalla(volatile unsigned int* ledBase, int width, int height) {
    for (int i = 0; i < width * height; i++)
        ledBase[i] = BLACK;
}

/**
 * isFreeZone:
 *   - Comprueba si un bloque 2×2 en la posición lineal `pos`
 *     está completamente libre (todos BLACK).
 *   - Útil antes de colocar la manzana para no solaparse con
 *     la serpiente ni con los bordes.
 *   - Devuelve 1 (verdadero) si todos los LEDs están en BLACK.
 */
int isFreeZone(volatile unsigned int* ledBase, int pos, int width) {
    return ledBase[pos]            == BLACK &&
           ledBase[pos + 1]        == BLACK &&
           ledBase[pos + width]    == BLACK &&
           ledBase[pos + width + 1]== BLACK;
}

/**
 * checkBoundary:
 *   - Dado un par de coordenadas (headX, headY),
 *     comprueba si están fuera de los límites [0..width-1]×[0..height-1].
 *   - Devuelve 1 si la cabeza se saldría de la matriz (colisión con borde).
 *   - Se invoca antes de mover la serpiente para detectar GAME OVER.
 */
int checkBoundary(int headX, int headY, int width, int height) {
    return headX < 0 || headX >= width ||
           headY < 0 || headY >= height;
}

/**
 * createNode:
 *   - Reserva dinámicamente un nuevo nodo de la serpiente.
 *   - Reserva un array de 4 punteros (`n->leds`) para un bloque 2×2.
 *   - Inicializa esos 4 punteros señalando a las 4 esquinas
 *     empezando en `blockBase`.
 *   - Fija `next = NULL`. Devuelve el nodo listo para insertarlo
 *     en la lista de la serpiente.
 *
 *   Nota: `blockBase` debe apuntar a la esquina superior izquierda
 *         del bloque 2×2 que representa el segmento de la serpiente.
 */
Node* createNode(volatile unsigned int* blockBase, int width) {
    Node* n = (Node*)malloc(sizeof(Node));
    n->leds = (volatile unsigned int**)malloc(4 * sizeof(volatile unsigned int*));
    n->leds[0] = blockBase;
    n->leds[1] = blockBase + 1;
    n->leds[2] = blockBase + width;
    n->leds[3] = blockBase + width + 1;
    n->next    = NULL;
    return n;
}

/**
 * delay_ms:
 *   - Genera un simple retardo aproximado de `ms` milisegundos
 *     usando un bucle de operación dummy en la CPU.
 *   - Se usa para controlar la velocidad de movimiento de la serpiente.
 */
void delay_ms(int ms) {
    int cnt;
    for (int i = 0; i < ms * LOOPS_PER_MS; i++) {
        cnt++;
    }
}

/**
 * freeSnake:
 *   - Recorre todos los nodos de la serpiente desde la cabeza a la cola.
 *   - Para cada nodo, libera primero el array dinámico `leds`,
 *     luego libera el propio nodo.
 *   - Finalmente libera la estructura `SnakeType`.
 *   - Garantiza que no quedan fugas de memoria al salir del juego.
 */
void freeSnake(SnakeType* snake) {
    Node* cur = snake->head;
    while (cur) {
        Node* next = cur->next;
        free(cur->leds);
        free(cur);
        cur = next;
    }
    free(snake);
}

/**
 * computeNewHeadBase:
 *   - Recibe el puntero al LED [0] del bloque actual de la cabeza,
 *     la dirección de movimiento y el ancho de la matriz.
 *   - Calcula y retorna el puntero a la esquina superior izquierda
 *     del **siguiente** bloque 2×2 en esa dirección.
 *   - Utilizado tanto en movimiento normal como en crecimiento
 *     para posicionar la nueva cabeza.
 */
volatile unsigned int* computeNewHeadBase(volatile unsigned int* oldHeadLed, motion currentDir, int width) {
    switch (currentDir) {
      case UP:    return oldHeadLed - 2*width;
      case DOWN:  return oldHeadLed + 2*width;
      case LEFT:  return oldHeadLed - 2;
      case RIGHT: return oldHeadLed + 2;
      default: return oldHeadLed;
    }
}


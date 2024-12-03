#include "I2C.h"
#include "ThisThread.h"
#include "mbed.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include <cstring>
#include <cmath>

// Definiciones y Pines
#define tiempo_muestreo 1s
#define HTU21D_ADDRESS 0x80  // Desplazado a la izquierda para escritura
#define HUMIDITY_MEASURE_CMD 0xE5
#define TEMP_MEASURE_CMD 0xE3
#define MEASURE_WAIT 50ms

// Alias TM1637
#define escritura       0x40
#define poner_brillo    0x88
#define dir_display     0xC0

BufferedSerial serial(USBTX, USBRX);
I2C i2c(D14, D15);
Adafruit_SSD1306_I2c oled(i2c, D0);
AnalogIn ain(A0);  // Sensor de voltaje (A0)
AnalogIn temp_resistive(A1);  // Sensor de temperatura resistivo (A1)

// Variables globales
float Vin = 0.0;
int ent = 0;
int dec = 0;
char men[40];

// Variables para HTU21D
float temperaturaHTU = 0.0;
float humedadHTU = 0.0;
float temperatura_resistiva = 0.0;
float error_absoluto = 0.0;
float error_relativo = 0.0;
const char* mensaje_inicio = "Arranque del programa\n\r";

// Vectores para almacenar las temperaturas
#define NUM_VALUES 10
float temperaturas_resistivas[NUM_VALUES];
float temperaturas_HTU[NUM_VALUES];
int index_resistiva = 0;
int index_HTU = 0;

// Pines del TM1637
DigitalOut sclk(D2);
DigitalInOut dio(D3);

// Segmentos para los números en el TM1637
const char digitToSegment[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

// Prototipos para TM1637
void condicion_start(void);
void condicion_stop(void);
void send_byte(char data);
void send_data(int number);

// Función para inicializar la pantalla OLED
void iniciar_oled() {
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(1);
    oled.display();
    ThisThread::sleep_for(3000ms);
    oled.clearDisplay();
    oled.display();
    oled.printf("Test\r\n");
    oled.display();
}

// Función para leer la humedad desde el sensor HTU21D
void leer_humedad() {
    char comando_humedad = HUMIDITY_MEASURE_CMD;
    i2c.write(HTU21D_ADDRESS, &comando_humedad, 1);
    ThisThread::sleep_for(MEASURE_WAIT);
    char data_humedad[2];
    i2c.read(HTU21D_ADDRESS + 1, data_humedad, 2);
    uint16_t humedad_raw = (data_humedad[0] << 8) | data_humedad[1];
    humedadHTU = (humedad_raw * 125.0 / 65536.0) - 6;
}

// Función para leer la temperatura desde el sensor HTU21D
void leer_temperatura_HTU() {
    char comando_temperatura = TEMP_MEASURE_CMD;
    i2c.write(HTU21D_ADDRESS, &comando_temperatura, 1);
    ThisThread::sleep_for(MEASURE_WAIT);
    char data_temp[2];
    i2c.read(HTU21D_ADDRESS + 1, data_temp, 2);
    uint16_t temp_raw = (data_temp[0] << 8) | data_temp[1];
    temperaturaHTU = (temp_raw * 175.72 / 65536.0) - 46.85;
}

// Función para leer la temperatura desde un sensor resistivo
void leer_temperatura_resistiva() {
    Vin = ain.read() * 3.3;  // Leer el voltaje
    ent = int(Vin);
    dec = int((Vin - ent) * 10000);
    float R2 = 10000;  // Resistencia de referencia (10kΩ)
    float R1 = R2 * (Vin / (3.3 - Vin));  // Calcular la resistencia del termistor

    if (R1 > 0) {  // Asegúrate de que R1 sea positiva
        float T0 = 298.15; // 25 °C en Kelvin
        float R0 = 10000;  // Resistencia a 25 °C
        float beta = 2864.327; // Beta del termistor

        // Calcular temperatura en Kelvin y luego convertir a Celsius
        float T = 1.0 / (1.0 / T0 + (1.0 / beta) * log(R1 / R0));
        temperatura_resistiva = T - 273.15;  // Convertir a Celsius
    } else {
        temperatura_resistiva = -999.0;  // Valor de error
    }
}

// Función para ordenar el vector usando el método de burbuja
void ordenar_burbuja(float* array, int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (array[j] > array[j + 1]) {
                float temp = array[j];
                array[j] = array[j + 1];
                array[j + 1] = temp;
            }
        }
    }
}

// Función para calcular la mediana del vector de temperaturas
float calcular_mediana(float* array, int n) {
    float temp_array[n];
    memcpy(temp_array, array, n * sizeof(float));
    ordenar_burbuja(temp_array, n);
    if (n % 2 != 0) {
        return temp_array[n / 2];
    } else {
        return (temp_array[(n - 1) / 2] + temp_array[n / 2]) / 2.0;
    }
}

// Función para calcular el promedio de dos valores
float calcular_promedio(float a, float b) {
    return (a + b) / 2.0;
}

// Función para calcular errores
void calcular_errores() {
    error_absoluto = fabs(temperaturaHTU - temperatura_resistiva);
    if (temperaturaHTU != 0) {
        error_relativo = (error_absoluto / temperaturaHTU) * 100.0;
    } else {
        error_relativo = 0.0;
    }
}

// Función para imprimir los datos de los vectores de temperaturas en la consola en unidades y decenas, ordenados
void imprimir_vectores_temperatura_ordenados() {
    // Crear copias de los vectores de temperatura
    float temp_resistiva_ordenado[NUM_VALUES];
    float temp_HTU_ordenado[NUM_VALUES];
    
    // Copiar los valores originales a los arreglos temporales
    memcpy(temp_resistiva_ordenado, temperaturas_resistivas, NUM_VALUES * sizeof(float));
    memcpy(temp_HTU_ordenado, temperaturas_HTU, NUM_VALUES * sizeof(float));

    // Ordenar los arreglos temporales usando el método de burbuja
    ordenar_burbuja(temp_resistiva_ordenado, NUM_VALUES);
    ordenar_burbuja(temp_HTU_ordenado, NUM_VALUES);

    // Imprimir el vector de temperaturas resistivas ordenado
    sprintf(men, "Vector Temp Resistiva Ordenado: ");
    serial.write(men, strlen(men));
    for (int i = 0; i < NUM_VALUES; i++) {
        int entero = static_cast<int>(temp_resistiva_ordenado[i]);
        int decimales = static_cast<int>((temp_resistiva_ordenado[i] - entero) * 100);
        sprintf(men, "%d.%02d ", entero, decimales);
        serial.write(men, strlen(men));
    }
    sprintf(men, "\n");
    serial.write(men, strlen(men));

    // Imprimir el vector de temperaturas HTU ordenado
    sprintf(men, "Vector Temp HTU Ordenado: ");
    serial.write(men, strlen(men));
    for (int i = 0; i < NUM_VALUES; i++) {
        int entero = static_cast<int>(temp_HTU_ordenado[i]);
        int decimales = static_cast<int>((temp_HTU_ordenado[i] - entero) * 100);
        sprintf(men, "%d.%02d ", entero, decimales);
        serial.write(men, strlen(men));
    }
    sprintf(men, "\n");
    serial.write(men, strlen(men));
}



// Función para mostrar los datos en la OLED y TM1637
void mostrar_datos() {
    oled.clearDisplay();
    oled.setTextCursor(0, 0);

    int tempIntHTU = static_cast<int>(temperaturaHTU);
    int humedadInt = static_cast<int>(humedadHTU);
    sprintf(men, "Temp: %d C Hum: %d %%\n", tempIntHTU, humedadInt);
    oled.printf(men);
    serial.write(men, strlen(men));

    int tempResistivaInt = static_cast<int>(temperatura_resistiva);
    sprintf(men, "Temp Ter: %d C\n", tempResistivaInt);
    oled.printf(men);
    serial.write(men, strlen(men));

    sprintf(men, "Volt Ter: %01u.%04u V\n", ent, dec);
    serial.write(men, strlen(men));

    calcular_errores();
    int unidades_abs = static_cast<int>(error_absoluto);
    int decimales_abs = static_cast<int>((error_absoluto - unidades_abs) * 100);
    int unidades_rel = static_cast<int>(error_relativo);
    int decimales_rel = static_cast<int>((error_relativo - unidades_rel) * 100);
    
    sprintf(men, "ErrA:%d.%02dC R:%d.%02d %%\n", unidades_abs, decimales_abs, unidades_rel, decimales_rel);
    oled.printf(men);
    serial.write(men, strlen(men));

    float mediana_resistiva = calcular_mediana(temperaturas_resistivas, NUM_VALUES);
    int medianaResistivaInt = static_cast<int>(mediana_resistiva);
    sprintf(men, "Mediana Temp Ter: %d C\n", medianaResistivaInt);
    serial.write(men, strlen(men));

    float mediana_HTU = calcular_mediana(temperaturas_HTU, NUM_VALUES);
    int medianaHTUInt = static_cast<int>(mediana_HTU);
    sprintf(men, "Mediana Temp HTU: %d C\n", medianaHTUInt);
    serial.write(men, strlen(men));

    // Calcular el promedio de las dos medianas y mostrar en TM1637
    float promedio = calcular_promedio(mediana_resistiva, mediana_HTU);
    int promedioInt = static_cast<int>(promedio);  // Redondear a un número entero
    send_data(promedioInt);  // Enviar el promedio al TM1637

    oled.display();

    // Llamar a la función para imprimir los vectores de temperaturas
     imprimir_vectores_temperatura_ordenados();
}

// Funciones de control para TM1637
void condicion_start(void) {
    dio.output();
    dio = 1;
    sclk = 1;
    dio = 0;
    sclk = 0;
}

void condicion_stop(void) {
    dio.output();
    dio = 0;
    sclk = 1;
    dio = 1;
}

void send_byte(char data) {
    dio.output();
    for (int i = 0; i < 8; i++) {
        dio = (data >> i) & 0x01;
        sclk = 1;
        sclk = 0;
    }
    dio.input();
    sclk = 1;
    sclk = 0;
    dio.output();
}

void send_data(int number) {
    int thousands = (number / 1000) % 10;
    int hundreds = (number / 100) % 10;
    int tens = (number / 10) % 10;
    int ones = number % 10;

    condicion_start();
    send_byte(escritura);
    condicion_stop();

    condicion_start();
    send_byte(dir_display);
    send_byte(digitToSegment[thousands]);
    send_byte(digitToSegment[hundreds]);
    send_byte(digitToSegment[tens]);
    send_byte(digitToSegment[ones]);
    condicion_stop();

    condicion_start();
    send_byte(poner_brillo);
    condicion_stop();
}

// Función principal
int main() {
    serial.write(mensaje_inicio, strlen(mensaje_inicio));
    iniciar_oled();

    while (true) {
        leer_temperatura_HTU();
        leer_humedad();
        leer_temperatura_resistiva();

        // Guardar temperaturas en los vectores
        temperaturas_resistivas[index_resistiva] = temperatura_resistiva;
        temperaturas_HTU[index_HTU] = temperaturaHTU;
        
        index_resistiva = (index_resistiva + 1) % NUM_VALUES;
        index_HTU = (index_HTU + 1) % NUM_VALUES;

        mostrar_datos();
        ThisThread::sleep_for(tiempo_muestreo);
    }
}

#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "I2Cbus.hpp"
#include "kalmanfilter.hpp"
#include <math.h>
#include <esp_system.h>
//variaveis globais
QueueHandle_t xQueue1;
QueueHandle_t xQueue2;
bool flag1,flag2;
uint32_t last1 = 0;
TickType_t xLastWakeTime;
TickType_t xLastWakeTime1;
TickType_t xLastWakeTime2;

//Prototipo das Funções
double funcaoPID(double output);
//Prototipos Threads
void vThread1(void *pvParameter);
void vThread2(void *pvParameter);
void vThread3(void *pvParameter);

   //funcao
double 
  kp = 144.0108, //55*0.6
  ki = 79.0073, //Ti = Pcr/2(0.5/2 = 0.25) //KI = kp/pcr
  kd = 8.3530,
  outputSum = 0.0,
  error = 0.0,
  deltaTime = 0.0,
  lastOutput = 0.0;
  int outMax = 400,
  outMin = -400;

  double funcaoPID(double output){

    double setPoint=+2.0;
    //deltaTime =0.008;
    
    //Error
    error = setPoint - output;
    
    //I
    outputSum+= (ki * error)*deltaTime;

    if(outputSum > outMax) outputSum= outMax;
    else if(outputSum < outMin) outputSum= outMin;

    //D
    double dOutput = ((output - lastOutput)*kd)/deltaTime;

    //P
    double outputPID = kp * error;

    outputPID += outputSum - kd * dOutput;

    if(outputPID > outMax) outputPID = outMax;
    else if(outputPID < outMin) outputPID = outMin;
    
    lastOutput = output;
    //lastTime = now;
    return outputPID;

      /*Remember some variables for next time*/
  }

//Main
  extern "C" void app_main() {
//void app_main() {
	//calibração do sensor
	//configuração do I2C
	/*
	ESP_ERROR_CHECK( myI2C.begin(GPIO_NUM_21, GPIO_NUM_22, 400000));
	myI2C.setTimeout(20);
	myI2C.writeBit(0x68, 0x6B, 6, 0);
	vTaskDelay(200 / portTICK_PERIOD_MS);

	//ler dados
	while(myI2C.readBytes(0x68, 0x3b, 6, buffer));
	ax = (int16_t)((buffer[0] << 8) | buffer[1]); // ([ MSB ] [ LSB ])
	ay = (int16_t)((buffer[2] << 8) | buffer[3]); // ([ MSB ] [ LSB ])
	az = (int16_t)((buffer[4] << 8) | buffer[5]); // ([ MSB ] [ LSB ])
	pitch = atan(ax/sqrt(ay * ay + az * az)) * RAD_TO_DEG;
	roll = atan(ay/sqrt(ax * ax + az * az)) * RAD_TO_DEG;
*/
	//Criação de Threads
   xTaskCreate(&vThread1, "MPU6050", 2048, NULL, 2, NULL);
   xTaskCreate(&vThread2, "Motor Esquerda", 2048, NULL, 1, NULL);
   xTaskCreate(&vThread3, "Motor Direita", 2048, NULL, 1, NULL);
   xQueue1 = xQueueCreate( 2, sizeof( double ) );
   xQueue2 = xQueueCreate( 2, sizeof( double ) );
 }

 void vThread1(void *pvParameter)
 {
	I2C_t& myI2C = i2c0;  // i2c0 and i2c1 are the default objects
	uint8_t buffer[14];
	double ax, ay, az;
	double gyroy,gy;
	//gyrox,gyroz,gx,gz
	double RAD_TO_DEG = 57.29577951;
	double pitch,fpitch;
	//roll,froll;
	double res;
	KALMAN pfilter(0.002);
	KALMAN rfilter(0.002);
	uint32_t lasttime = 0;
	uint32_t count = 0;
	uint32_t last1 = 0;
	BaseType_t xStatus1;
	BaseType_t xStatus2;

	ESP_ERROR_CHECK( myI2C.begin(GPIO_NUM_21, GPIO_NUM_22, 400000));
	vTaskDelay(200 / portTICK_PERIOD_MS);
	buffer[0] = 7; //1khz
	buffer[1] = 0x00; //dlpf desativado
	buffer[2] = 0x00; //fundo de escala gyro 250 graus/s
	buffer[3] = 0x00;//gyro +-2g
	myI2C.setTimeout(10);
	myI2C.writeBit(0x68, 0x6B, 6, 0);//desativar modo sleep
  vTaskDelay(200 / portTICK_PERIOD_MS);
  myI2C.writeBytes(0x68, 0x19,4,buffer);
  vTaskDelay(200 / portTICK_PERIOD_MS);

  xLastWakeTime = xTaskGetTickCount();

  while(1)
  {
    ESP_ERROR_CHECK(myI2C.readBytes(0x68, 0x3b, 12, buffer));
  		ax = (int16_t)((buffer[0] << 8) | buffer[1]); // ([ MSB ] [ LSB ])
  		ay = (int16_t)((buffer[2] << 8) | buffer[3]); // ([ MSB ] [ LSB ])
  		az = (int16_t)((buffer[4] << 8) | buffer[5]); // ([ MSB ] [ LSB ])

  	//	gyrox = (int16_t)((buffer[8] << 8) | buffer[9]); // ([ MSB ] [ LSB ])
  		gyroy = (int16_t)((buffer[10] << 8) | buffer[11]); // ([ MSB ] [ LSB ])
  	//	gyroz = (int16_t)((buffer[12] << 8) | buffer[13]); // ([ MSB ] [ LSB ])

  		pitch = atan(ax/sqrt(ay * ay + az * az)) * RAD_TO_DEG;
  	//	roll = atan(ay/sqrt(ax * ax + az * az)) * RAD_TO_DEG;

  		gy = gyroy / 131.0;
  	//	gx = gyrox / 131.0;
  	//	gz = gyroz / 131.0;

  		fpitch = pfilter.filter(-pitch, gy);
  	//	froll = rfilter.filter(-roll, gx);

  	/*
  		xLastWakeTime = xTaskGetTickCount();
  		if((xLastWakeTime-last1)>=100){
  		last1 = xLastWakeTime;
  		ESP_LOGI("mpu6050", "Samples: %d", count);	
  		}
	*/
  		count++;
  		if(esp_log_timestamp() / 1000 != lasttime) {
  			lasttime = esp_log_timestamp() / 1000;
  			deltaTime = (1.0/count);
  	//		ESP_LOGI("mpu6050", "Samples: %d", count);
  			ESP_LOGI("mpu6050", "deltaTime: %lf", deltaTime);
  			count = 0;
  	//		ESP_LOGI("mpu6050", "xLastWakeTime ( %d)",xLastWakeTime);
  	//		ESP_LOGI("mpu6050", "Acc: ( %.3lf, %.3lf, %.3lf)", ax, ay, az);
  	//		ESP_LOGI("mpu6050", "Gyro: ( %.3f, %.3f, %.3f)", gx, gy, gz);
  	//		ESP_LOGI("mpu6050", "Pitch: %.3lf", pitch);
  	//		ESP_LOGI("mpu6050", "Erro: %.3lf", error);
  			ESP_LOGI("mpu6050", "FPitch: %.3lf", fpitch);
  	//		ESP_LOGI("mpu6050", "FRoll: %.3lf", froll);
  			ESP_LOGI("PID", "RES: %.3lf", res);
  		}
  		res = funcaoPID(fpitch);
      //if (res < 200) res = res*3;
  		xStatus1 = xQueueSendToBack( xQueue1, &res, pdMS_TO_TICKS( 0 ) );
  		xStatus2 = xQueueSendToBack( xQueue2, &res, pdMS_TO_TICKS( 0 ) );
  		if( xStatus1 != pdPASS )
  		{
  			printf( "A fila do Motor Esquerda está cheia.\r\n" );
  		}
  		else
        flag1 = true;

      if( xStatus2 != pdPASS )
      {
       printf( "A fila do Motor Direita está cheia.\r\n" );
     }
     else
      flag2 = true;
    vTaskDelayUntil( &xLastWakeTime, 1 );
  }
}


  void vThread2(void *pvParameter) // Motor Esquerda
  {
  	//variaveis
//  	uint32_t count = 0;
//  	uint32_t lasttime = 0;
  	double res;
  	BaseType_t xStatus;
//  configuração do timer0
    /*
     * Prepare and set configuration of timers
     * that will be used by LED Controller
     */
  	ledc_timer_config_t ledc_timer;
    ledc_timer.duty_resolution = LEDC_TIMER_10_BIT, // resolution of PWM duty
    ledc_timer.freq_hz = 5000,                      // frequency of PWM signal
    ledc_timer.speed_mode = LEDC_HIGH_SPEED_MODE,           // timer mode
    ledc_timer.timer_num = LEDC_TIMER_0; //index 


  // Set configuration of timer0 for high speed channels
    ledc_timer_config(&ledc_timer);


      // Configurar canal PWM 0
    ledc_channel_config_t ledc_channel0;
    ledc_channel0.channel    = LEDC_CHANNEL_0,
    ledc_channel0.duty       = 500,
    ledc_channel0.gpio_num   = 4,
    ledc_channel0.speed_mode = LEDC_HIGH_SPEED_MODE,
    ledc_channel0.timer_sel  = LEDC_TIMER_0;

// Configurar canal PWM 1
    ledc_channel_config_t ledc_channel1;
    ledc_channel1.channel    = LEDC_CHANNEL_1,
    ledc_channel1.duty       = 500,
    ledc_channel1.gpio_num   = 5,
    ledc_channel1.speed_mode = LEDC_HIGH_SPEED_MODE,
    ledc_channel1.timer_sel  = LEDC_TIMER_0;

    //  Setar canais 
    ledc_channel_config(&ledc_channel0);
    ledc_channel_config(&ledc_channel1);

    //  Fade
    //    ledc_fade_func_install(0); 
    xLastWakeTime1 = xTaskGetTickCount();

    while(1) {
        	/*
        	count++;
        	if(esp_log_timestamp() / 1000 != lasttime) {
        		lasttime = esp_log_timestamp() / 1000;
        		ESP_LOGI("Thread Motor Esquerda", "Samples: %d", count);
        		count = 0;
        	}
        	*/
    	if(flag1){
    		xStatus = xQueueReceive( xQueue1, &res, 0 );
    		if( xStatus != pdPASS )
    		{
    			printf( "A fila do Motor Esquerda está vazia.\r\n" );
    		}
    		else{
        //			ESP_LOGI("Motor Esquerda", "RES: %.3lf", res);

    			if(res > 0){

            ESP_ERROR_CHECK( ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0) ); 
            ESP_ERROR_CHECK( ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0) );

            ESP_ERROR_CHECK( ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, abs(res)) );
            ESP_ERROR_CHECK( ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1) );
          }
          else{

            ESP_ERROR_CHECK( ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, abs(res)) ); 
            ESP_ERROR_CHECK( ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0) );

            ESP_ERROR_CHECK( ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, 0) );
            ESP_ERROR_CHECK( ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1) );
          }
          flag1=false;
        }
      }
    // vTaskDelayUntil( &xLastWakeTime1, 1 );
    }
  }

    void vThread3(void *pvParameter) // Motor Direita
    {

    //variaveis
    //	uint32_t count = 0;
    //	uint32_t lasttime = 0;
    	double res;
    	BaseType_t xStatus;

        //configurar timer 1
    	ledc_timer_config_t ledc_timer;
        ledc_timer.duty_resolution = LEDC_TIMER_10_BIT, // resolution of PWM duty
        ledc_timer.freq_hz = 5000,                      // frequency of PWM signal
        ledc_timer.speed_mode = LEDC_HIGH_SPEED_MODE,           // timer mode
        ledc_timer.timer_num = LEDC_TIMER_1; //index 

  // Set configuration of timer0 for high speed channels
        ledc_timer_config(&ledc_timer);

        // Configurar canal PWM 2
        ledc_channel_config_t ledc_channel2;
        ledc_channel2.channel    = LEDC_CHANNEL_2,
        ledc_channel2.duty       = 500,
        ledc_channel2.gpio_num   = 18,
        ledc_channel2.speed_mode = LEDC_HIGH_SPEED_MODE,
        ledc_channel2.timer_sel  = LEDC_TIMER_1;
// Configurar canal PWM 3
        ledc_channel_config_t ledc_channel3;
        ledc_channel3.channel    = LEDC_CHANNEL_3,
        ledc_channel3.duty       = 500,
        ledc_channel3.gpio_num   = 19,
        ledc_channel3.speed_mode = LEDC_HIGH_SPEED_MODE,
        ledc_channel3.timer_sel  = LEDC_TIMER_1;
      //setar canais
        ledc_channel_config(&ledc_channel2);
        ledc_channel_config(&ledc_channel3);

        xLastWakeTime2 = xTaskGetTickCount();

        while(1){
        	/*
        	count++;
        	if(esp_log_timestamp() / 1000 != lasttime) {
        		lasttime = esp_log_timestamp() / 1000;
        		ESP_LOGI("Thread Motor Direita", "Samples: %d", count);
        		count = 0;
        	}
        	*/
        	if(flag2){
        		xStatus = xQueueReceive( xQueue2, &res, 0 );
        		if( xStatus != pdPASS )
        		{
        			printf( "A fila do Motor Direita está vazia.\r\n" );
        		}
        		else{
        //	ESP_LOGI("Motor Direita", "FPitch: %.3lf", res);
        			if(res > 0){

                ESP_ERROR_CHECK( ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_2, 0) ); 
                ESP_ERROR_CHECK( ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_2) );

                ESP_ERROR_CHECK( ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_3, abs(res)) );
                ESP_ERROR_CHECK( ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_3) );
              }
              else{

                ESP_ERROR_CHECK( ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_2, abs(res)) ); 
                ESP_ERROR_CHECK( ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_2) );

                ESP_ERROR_CHECK( ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_3, 0) );
                ESP_ERROR_CHECK( ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_3) );
              }
              flag2=false;
            }
          //  vTaskDelayUntil( &xLastWakeTime2, 1 );
          }
        }
      }



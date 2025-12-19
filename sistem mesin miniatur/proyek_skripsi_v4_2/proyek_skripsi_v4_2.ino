//library arduino
#include <ACS712.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <EncoderStepCounter.h>

//pin module arduino
int pin_button_on = 10; //pin button
int pin_button_off = 11;
SoftwareSerial bluetooth(2, 3); //pin bluetooth: RX, TX
#define CLK 4 //pin rotary encoder putaran CLK dan DT
#define DT 5
EncoderStepCounter REncoder(CLK, DT);
#define SW 6 //pin tombol rotary encoder
int pin_relay = 7; //Pin untuk relay
// int pin_button_mode = 8; //pin untuk ganti mode komunikasi
// SoftwareSerial sms_gateway(9, 8); //pin SIM800L: 0/RX -> TX, 1/TX -> RX -digantikan menggunakan hardware serial
#define VOLTAGE_IN_PIN A0 //pin sensor tegangan
ACS712 sensor_arus(ACS712_30A, A1); //pin sensor arus
LiquidCrystal_I2C lcd(0x27, 16, 2); //setting module lcd i2c

// Variable untuk storing incoming value seperti state module
int state_button_on; //state interface button on dan off
int state_button_off;
int state_relay = HIGH; //state relay
int state_button_mode; // state button ganti mode komunikasi
char incoming_value; //state char masukan bluetooth
// String incoming_value; //state String masukan bluetooth - rusak
float adc_volt = 0.0; //float variabel adc
float in_volt = 0.0; //float input voltage
float R1 = 30000.0; //nilai resistor R1
float R2 = 7500.0; //dan R2 divider sensor tegangan dc
float ref_volt = 5.0; //referensi sensor voltage sensor tegangan
int adc_val = 0; //integer value adc untuk baca listrik
float arus = 0.0; //value arus listrik
int state_button_sw; //state button SW rotary encoder
int counter_button_sw = 0; //counter sw encoder
float counterA = 20.0; //counter ampere encoder
// int countdownTime = 0; //timer
// signed char state_rotary_last = 0;
// bool error_pengaman; //boolean pengaman

// nilai boolean sensor 
bool error_tegangan = false;
float butuh_tegangan = 6.0;
bool error_arus = false;
float limit_arus = 20.0;
bool notif_toleransi = false;
float toleransi_arus = 0.7;
bool mesin_normal = true; //variabel untuk menghentikan user menyalakan mesin saat ada kerusakan mesin
bool delay_starter_pengaman = true; //delay sensor untuk lonjakan listrik awal
// boolean untuk pengecekan pertama dan setting
// bool sensor_loop = true;
bool show_A_once = true;
bool kirim_error_flutter_satukali = true;
bool kirim_notifikasi_on = true;
bool kirim_notifikasi_on_toleransi = false;
bool kirim_notifikasi_off = true;
// bool modeSMS = false;
// bool rubah_BT_run = false;
// bool rubah_SMS_run = false;

void setup() {
  // Sets the data rate in bits per second (baud)
  Serial.begin(9600); //untuk transmisi module gsm sim800l
  // sms_gateway.begin(9600); //untuk debug testing module gsm
  bluetooth.begin(9600); //untuk transmisi data bluetooth
  lcd.begin(); //setup module lcd
  lcd.backlight();
	lcd.print(" Menyiapkan...  ");
  pinMode(pin_relay, OUTPUT); //set pin relay ke output
  digitalWrite(pin_relay, HIGH); //inisialisasi relay ke HIGH (off/mati)
  pinMode(pin_button_on,INPUT_PULLUP); //button on
  pinMode(pin_button_off, INPUT_PULLUP); //button off
  sensor_arus.calibrate(); //kalibrasi sensor arus
  REncoder.begin(); //input rotary encoder
  pinMode(SW,INPUT_PULLUP); //button encoder
  delay(2000); //delay untuk aktivasi module sim800l normal
  Serial.println("AT"); //tes handshake koneksi module
  // updateSerialSIM(); //untuk debug module sim800l
  delay(100);
  lcd.clear();
	lcd.print(" Mesin DiskMill ");
}

void loop() {
  // Put your main code here, to run repeatedly:
  //BAGIAN ANTARMUKA LCD, ROTARY ENCODER, INISIALISASI PENGAMAN, KIRIM DATA ERROR
  // lcd.clear();
  if (state_relay == HIGH) {
    delay_starter_pengaman = true;
    // bluetooth.write("n0"); //n0 = mati
    state_button_sw = digitalRead(SW); //function untuk update counter encoder
    if (state_button_sw == LOW) {
      counter_button_sw++;
      // Serial.println(counter_button_sw); //untuk debug
      delay(200);
    }
    if (counter_button_sw == 1) { //setting Ampere analog
      if (show_A_once == true) { //kode print LCD ini membuat fungsi encoder ngebug tanpa IF untuk interaksi awal dengan user
        lcd.setCursor(0, 1);
        lcd.print(String("LimitA: ") + counterA + String("A")); 
        // Serial.println(counterA); // untuk debugging
        show_A_once = false;
      }
      REncoder.tick();
      signed char state_rotary_A = REncoder.getPosition();
      if (state_rotary_A != 0) {
        counterA = counterA + state_rotary_A;
        counterA = constrain(counterA, 0, 30);  
        REncoder.reset();
        lcd.setCursor(0, 1); //interface update di LCD
        lcd.print(String("LimitA: ") + counterA + String("A"));
        // Serial.println(counterA); //untuk debug keluaran
      }
    } else if (counter_button_sw == 2) { //kembali ke awal
      lcd.setCursor(0, 1);
      lcd.print("                ");
      counter_button_sw = 0;
      show_A_once = true;
    }
  } else if (state_relay == LOW) {
    // bluetooth.write("n1"); //n1 = hidup
    if (delay_starter_pengaman == true) {
      delay(3000);
      delay_starter_pengaman = false;
    }
    deteksi_pengaman();
    // notifikasi jika arus listrik 80% saat mesin menyala
    if (notif_toleransi == true) {
      bluetooth.write("n2");
      lcd.setCursor(0, 0);
      lcd.println("Arus Berlebih!  ");
      kirim_notifikasi_on_toleransi = true;
    } else if (notif_toleransi == false && kirim_notifikasi_on_toleransi == true) {
      bluetooth.write("n1");
      lcd.setCursor(0, 0);
      lcd.println("Mesin on        ");
      kirim_notifikasi_on_toleransi = false;
    }
    // else {
    //   bluetooth.println("Mesin Menyala.");
    // }
    // // reset untuk fungsi error
    // kirim_error_flutter_satukali = true;
    // function untuk update counter encoder
    state_button_sw = digitalRead(SW);
    // tombol alih timer ke ampere
    if (state_button_sw == LOW) {
      counter_button_sw++;
      // Serial.println(counter_button_sw);
      delay(200);
    }
    if (counter_button_sw == 1) {
      lcd.setCursor(0, 0);
      lcd.println(String("ON | V:") + in_volt);
    } else if (counter_button_sw == 2) {
      lcd.setCursor(0, 0);
      lcd.println(String("ON | A:") + arus);
    } else if (counter_button_sw == 3) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Mesin on        ");
      counter_button_sw = 0;
    }
  }
  //phase failure dan pengiriman data kerusakan ke aplikasi flutter
  if (error_tegangan == true || error_arus == true) { //kondisi pengaman
    digitalWrite(pin_relay, HIGH);
    if (error_tegangan == true) {
      mesin_normal = false;
      // bluetooth.println("Error Tegangan Listrik (ERR_T)");
      lcd.setCursor(0, 0);
      lcd.print("ERR Tegangan    ");
      if (kirim_error_flutter_satukali == true) { //bugfix looping kirim error ke database flutter tanpa henti
        bluetooth.write("e1"); //e1 = error 1 tegangan
        kirimSMS("Tegangan listrik mesin bermasalah, segera diperbaiki.");
        delay(1000); //delay untuk proses sms
        kirim_error_flutter_satukali = false;
      }
      // deteksi_pengaman();
    } else if (error_arus == true) {
      mesin_normal = false;
      // bluetooth.println("Error Arus Listrik (ERR_A)");
      lcd.setCursor(0, 0);
      lcd.print("ERR Arus        ");
      if (kirim_error_flutter_satukali == true) {
        bluetooth.write("e2"); //e2 = error 2 arus listrik
        kirimSMS("Arus listrik melebihi batas maksimum yang diizinkan.");
        delay(1000); //delay untuk proses sms
        kirim_error_flutter_satukali = false;
      }
      // deteksi_pengaman();
    }
    deteksi_pengaman(); //sensor terus mendeteksi meskipun mesin dimatikan untuk memberitahu pengguna
  }
  //BAGIAN KONTROL BLUETOOTH
  state_relay = digitalRead(pin_relay);
  limit_arus = counterA; //memasukan data limit arus baru ke variabel
  if (mesin_normal == true && bluetooth.available() > 0) { //jika tersambung bluetooth perintah dibawah berjalan
    incoming_value = bluetooth.read(); //baca the transmisi data dari bluetooth lalu store ke variable
    // Serial.println(incoming_value); //print Value incoming_value di serial monitor untuk debug
    //cek apakah value incoming_value dengan IF ELSE untuk komunikasi bluetooth
    if (incoming_value == '0' && state_relay == LOW) {
      digitalWrite(pin_relay, HIGH); // If value is 0, turn OFF the device
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Mesin off       ");
      if (kirim_notifikasi_off == true) {
        bluetooth.write("n0");
        kirim_notifikasi_off = false;
      }
      // bluetooth.println("Mesin Dimatikan.");
      // incoming_value = ''; // bugfix reset transmisi untuk tidak kirim pesan ke flutter terus menerus
      kirim_notifikasi_on = true;
      delay(300); //diberi delay untuk stabilisasi transmisi
    } else if (incoming_value == '1' && state_relay == HIGH) {
      digitalWrite(pin_relay, LOW); // If value is 1, turn ON the device  
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Mesin on        ");
      if (kirim_notifikasi_on == true) {
        bluetooth.write("n1");
        kirim_notifikasi_on = false;
      }
      // bluetooth.println("Mesin Menyala.");
      // incoming_value = ''; // bugfix reset transmisi untuk tidak kirim pesan ke flutter terus menerus
      show_A_once = true; //bugfix lcd mesin
      kirim_notifikasi_off = true;
      delay(300);
    } else if (incoming_value == 'a' && state_relay == HIGH) { //tambahi limit_arus
      counterA = counterA + 1;
      counterA = constrain(counterA, 0, 30);
      lcd.setCursor(0, 1); //interface update di LCD
      lcd.print(String("LimitA: ") + counterA + String("A"));
      // Serial.println(limit_arus); //untuk debug
      bluetooth.println(String("Batas Arus: ") + floor(counterA) + String("V")); //percobaan kirim data pesan ke flutter versi 4
      // incoming_value = ''; // bugfix reset transmisi untuk tidak kirim pesan ke flutter terus menerus
      delay(300);
    } else if (incoming_value == 'b' && state_relay == HIGH) { //tambahi limit_arus
      counterA = counterA - 1;
      counterA = constrain(counterA, 0, 30);
      lcd.setCursor(0, 1); //interface update di LCD
      lcd.print(String("LimitA: ") + counterA + String("A"));
      // Serial.println(limit_arus); //untuk debug
      bluetooth.println(String("Batas Arus: ") + floor(counterA) + String("V")); //percobaan kirim data pesan ke flutter versi 4
      incoming_value = ""; // bugfix reset transmisi untuk tidak kirim pesan ke flutter terus menerus
      delay(300);
    } else if (incoming_value == 'x' && state_relay == HIGH) { //tombol alih mode sms tidak jadi dipakai karena pindah memakai hardware serial
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Mengaktifkan SMS");
      bluetooth.println("Mengaktifkan Mode SMS");
      delay(300);
      // rubah_SMS_run = true;
      // incoming_value == ' ';
      // modeSMS = true;
    } 
  }
  //BAGIAN KONTROL FISIK, logic antarmuka mesin memakai button analog
  state_button_on = digitalRead(pin_button_on);
  state_button_off = digitalRead(pin_button_off);
  if (mesin_normal == true) {
    if (state_button_off == LOW && state_relay == LOW) { // tombol off normal
      digitalWrite(pin_relay, HIGH);
      counter_button_sw = 0; // bugfix saat counter sw lebih dari 0 saat off mesin
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Mesin off       ");
      if (kirim_notifikasi_off == true) { //percobaan untuk mengurangi bug inteferensi sinyal
        bluetooth.write("n0");
        kirim_notifikasi_off = false;
      }
      kirim_notifikasi_on = true;
      // bluetooth.println("Mesin Dimatikan.");
    } else if (state_button_on == LOW && state_relay == HIGH) { // tombol on
      digitalWrite(pin_relay, LOW);
      show_A_once = true; //bugfix switch rotary encoder
      counter_button_sw = 0; // reset counter setting bugfix
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Mesin on        ");
      if (kirim_notifikasi_on == true) {
        bluetooth.write("n1");
        kirim_notifikasi_on = false;
      }
      kirim_notifikasi_off = true;
      // bluetooth.println("Mesin Menyala.");
    }
  }
  delay(1); //delay untuk stabilisasi loop
}
void deteksi_pengaman() {
  float volt_req_mati = 0.4;
  arus = constrain((sensor_arus.getCurrentDC() + 0.22), 0, 40); //bugfix dengan constrain mencegah value arus dari noise bilangan decimal negatif
  adc_val = analogRead(VOLTAGE_IN_PIN);
  adc_volt = (adc_val * ref_volt) / 1024.0;
  in_volt = adc_volt / (R2/(R1+R2));
  // Serial.println(String("Masukan Listrik = ") + in_volt + String("V"));
  // Serial.println(String("Arus Mengalir   = ") + arus + String("A"));
  // Serial.println(String("V:") + in_volt + String(" A:") + arus + String(" lA:") + limit_arus); // untuk debug
  lcd.setCursor(0, 1);
  lcd.print(in_volt + String("V"));
  lcd.setCursor(8, 1);
  lcd.print(arus + String("A"));
  if (in_volt < butuh_tegangan) { //phase-failure jika ada anomali listrik diputus.
    error_tegangan = true;
  }
  if (arus > limit_arus) { //masalah bila value arus keluar negatif dianggap lebih dari batasnya - sudah diperbaiki
    error_arus = true;
  }
  if (arus >= (limit_arus * toleransi_arus) && arus < limit_arus) { //peringatan arus listrik 80%
    notif_toleransi = true;
  } else {
    notif_toleransi = false;
  }
  delay(500); //delay untuk stabilisasi sensor
}
void kirimSMS(String pesan) {
  Serial.println("AT+CMGF=1"); //konfigurasi mode teks
  delay(100);
  Serial.println("AT+CMGS=\"+6281252848960\""); //setting nomor penerima sms
  delay(100);
  Serial.println(pesan); //isi pesan notifikasi
  delay(100);
  Serial.write(26); //eksekusi kirim sms
}
// //untuk debugging sim800l, hapus atau dikomen setelah/jika tidak digunakan
// void updateSerialSIM() {
//   delay(50);
//   while (Serial.available()) {
//     sms_gateway.write(Serial.read());
//   }
//   while (sms_gateway.available()) {
//     Serial.write(sms_gateway.read());
//   }
// }
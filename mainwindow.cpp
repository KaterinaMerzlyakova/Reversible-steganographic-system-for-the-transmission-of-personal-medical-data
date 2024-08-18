#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qfiledialog.h"
#include <QDebug>
#include <QImage>
#include <QThread>
#include <QRgb>
#include <QtCore/qmath.h>
#include <QFile>
#include <qmath.h>
#include <QMessageBox>
#include <QProgressDialog>
#include <QRandomGenerator>
#include <QTextBrowser>
#include <QStack>

#define Max 262144 //количество точек для картинки размером 512Х512.
#define Fields_Max 10000 //максимальное количество областей
#define Points_Max 8912// 4096 //максимальное количество точек в области
#define min 3 //минимально учитываемая область в исходной картинке
#define min_inp min*3 //для интерполированной
#define h 256
#define w 256
#define Top_value (((long) 1 << Cbits) -1)
#define Q1 (Top_value/4+1)
#define half (2*Q1)
#define Q3 (3*Q1)
#define NumC 2
#define EOF_symbol (NumC+1)
#define NumS (NumC+1)
#define Size_Comb 32
#define Size_Circle 5
#define BIT 7
#define Bound 1 //для Энтропии
#define Cbits 16 // биты кода
typedef long Tcode; // тип арифметического кода
static Tcode low, high;
int IndexC[NumC];
int Index[NumS+1];
#define Max_f 16383
int Freq_Cum[NumS+1];
unsigned char Key[Max];//массив псевдослучайного ключа

struct Probability //вероятности по областям
{
int nbt[2][Size_Comb];
};

Probability Prob[Fields_Max];

struct Points //точки областей
{
short int X;
short int Y;
};

QStack<Points> Stack;//стек точек

int Labels[H][W];//массив меток областей
int Regions;//количество областей
Points RegionList[Fields_Max][Points_Max];
Points RegionList_inp[Fields_Max][Points_Max*3];
int Sizes[Fields_Max];
int Sizes_inp[Fields_Max];
int Pixels_pict[h][w];
int Pixels_interp[H][W];
QFile file_mess, file_dec;
int Mess[Max]; //массив битов зашифрованного сообщения
int Source[Max]; //массив битов сообщения
int Dec[Max];//массив секретных битов из контейнера
int Write_bits; //счетчик записанных кодовых бит
int Read_bits; //счетчик бит источника сообщения
static long F_bits;
short hei, wid;
int freq[NumS+1]; //частоты символов
int Circle[Size_Circle]; //окружение из 5 бит
int Comb[2][Size_Comb];
double AverageEnt=0;

void Get_Key(){
    QRandomGenerator generator;
    for(int i=0;i<Max;i++) Key[i]=QRandomGenerator::global()->bounded(2);
}
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    Get_Key();//новый ключ генерируется при запуске
    ui->textEdit->append("Ключ сгенерирован.");
    ui->textEdit->append("Выберите файл для встраивания сообщения.");
}

MainWindow::~MainWindow()
{
    delete ui;
}

short bit(unsigned char P){
    unsigned char b;
    b=P<<7;
    b=b>>7;//младший бит пикселя
return b;
}

//бит по номеру r
short bit_on(unsigned char r, unsigned char P){
    unsigned char bts[9], b=0,q;
    for(q=1; q<=8; q++){
        bts[q]=P;
        bts[q]=bts[q]<<(q-1);
        bts[q]=bts[q]>>7;
    }
    b=bts[r];
return b;
}

void Read_pixels(QString str) {
   short i,j;
   QImage file_empt;
   file_empt.load(str);//загрузка bmp файла по имени из строки str

   hei=file_empt.height();//высота картинки
   wid=file_empt.width();//ширина картинки

   for(i=0;i<hei;++i){
       for(j=0;j<wid;++j) {
       Pixels_pict[i][j]=file_empt.pixelIndex(j,i);//запись значения пикселя
       }
                    }

}

//функция сравнения младшего бита точки P и бита сообщения b
bool Check_Bit(unsigned char P, unsigned char b)
{
P=P<<7;
P=P>>7;
return (P==b);
}

//Interpolation by Neighboring Pixels (INP)
void MainWindow::INP(QString f){
    short m,n,i,j;
QImage file_inp(H, W, QImage::Format_Indexed8);
QVector<QRgb> palette;
QRgb rgb;
for (i = 0; i < h; ++i) { rgb=qRgb(i,i,i); file_inp.setColor(i,rgb); } //устанавливается палитра цветов

    for(m=0;m<hei;m++){
        for(n=0;n<wid;n++) {

           i=2*m; j=2*n; Pixels_interp[i][j]=Pixels_pict[m][n];//заполнение опорными точками матрицы пикселей для интерполированного изображения
        }}

    for(i=0;i<H;i+=2){ //четные строки и нечетные столбцы
        for(j=1;j<W-1;j+=2) { Pixels_interp[i][j]=floor((Pixels_interp[i][j-1]+(Pixels_interp[i][j-1]+Pixels_interp[i][j+1])/2)/2);// точка С[0,1], C[2,1]...
    }}


    for(i=0;i<H;++i){ Pixels_interp[i][512-1]=Pixels_interp[i][j-1];//крайний правый столбец
    }

    for(i=1;i<H-1;i+=2){ //нечетные строки и четные столбцы
        for(j=0;j<w*2;j+=2) { Pixels_interp[i][j]=floor((Pixels_interp[i-1][j]+(Pixels_interp[i-1][j]+Pixels_interp[i+1][j])/2)/2); // точка С[1,0], C[1,2]...
    }}

   for(j=0;j<H;++j){ Pixels_interp[hei*2-1][j]=Pixels_interp[i-1][j];//крайняя нижняя строка
   }
   for(i=1;i<H;i+=2){ //нечетные строки и нечетные столбцы
        for(j=1;j<W;j+=2) { Pixels_interp[i][j]=floor((Pixels_interp[i-1][j]+Pixels_interp[i][j-1])/2); // точка С[1,1]
    }}

for(i=0;i<H;i++){
   for(j=0;j<W;j++) { file_inp.setPixel(i,j,Pixels_interp[j][i]);

   }}

  file_inp.save(f,"BMP");
}

double lg2(double osn) {return log(osn)/log(2);}

//определение нужного счетчика
short Get_Index(){
    unsigned char bt, q;
    for(q=1;q<=Size_Circle; q++)
        bt=bt+pow(2,Size_Circle-q)*Circle[q-1];
    return bt;
}

void Init(short f){//обнуление вероятностей и окружения
 short i,j;

 for(i=0; i<2; i++)
     for(j=0; j<Size_Comb; j++) {
         Prob[f].nbt[i][j]=1;
      }
 for(i=0; i<=Size_Circle; i++) Circle[i]=0;
}


void MainWindow:: Statistics(short f)// функция сбора статистики по области номер f
{
int i, j, p, ind_1;

   //qDebug()<<"Size_inp="<<Sizes[f];
//область Area размером k
for(p=0;p<Sizes[f];p++) {
    i=RegionList[f][p].X;//индекс строки
    j=RegionList[f][p].Y;//индекс столбца

   // qDebug()<<"i,j="<<i<<" "<<j;
    Circle[0]=bit_on(BIT-1, Pixels_pict[i][j]);
    Circle[1]=bit_on(BIT, Pixels_pict[i][j]);
    Circle[2]=bit_on(BIT-2, Pixels_pict[i][j]);
    Circle[3]=bit_on(BIT+1, Pixels_pict[i][j-1]);
    Circle[4]=bit_on(BIT+1, Pixels_pict[i][j+1]);

    ind_1=Get_Index();//число из собранных бит

    if(bit_on(8, Pixels_pict[i][j])==0)  Prob[f].nbt[0][ind_1]+=2; else Prob[f].nbt[1][ind_1]+=2;
    }
}

//замена младшего бита
short Swap(unsigned char P, unsigned char b){
   unsigned char q, P1=0, bits[9];
    for(q=1;q<=8;q++){
    bits[q]=P;
    bits[q]=bits[q]<<q-1;
    bits[q]=bits[q]>>7;
    }
    bits[8]=b;
    for(q=1;q<=8;q++) P1=P1+pow(2,8-q)*bits[q];
 return P1;
}

//функция записи заполненного контейнера
void Write_Pix(QString file, short f){
    int i,j,p;
    unsigned char Pix;
    QImage file_full;
    file_full.load(file);//загрузка bmp файла по имени из строки f
Write_bits=Write_bits-Sizes_inp[f];//взятие бит сообщения из массива с нужной точки
//область номер f
    for(p=0;p<Sizes_inp[f];p++) {
        i=RegionList_inp[f][p].X;//индекс строки
        j=RegionList_inp[f][p].Y;//индекс столбца

        if (!Check_Bit(Pixels_interp[i][j],Mess[Write_bits])) {
           Pix=Swap(Pixels_interp[i][j],Mess[Write_bits]);
           //qDebug()<<"Pix="<<Pix;
           file_full.setPixel(j,i,Pix);//запись пикселя
        }
         Write_bits++;
        }
  file_full.save(file,"BMP");
}

void Chek(QString str) {
   short i,j;
   QImage file_;
   file_.load(str);//загрузка bmp файла по имени из строки str

   hei=file_.height();//высота картинки
   wid=file_.width();//ширина картинки

   for(i=0;i<hei;++i){
       for(j=0;j<wid;++j) {
       qDebug()<<file_.pixelIndex(j,i)<<" i="<<i<<" j="<<j;//значение пикселя
       }
                    }
}

//считывание бит сообщения в массив
void MainWindow:: read_mess(){
    unsigned char t, bt, buff,cnte=0;
        char buffer;
     for(int i=0; i<Max;i++){
      if(cnte==0) { // чтение байта если буфер пуст
       file_mess.getChar(&buffer);// взяли байт из массива
       buff=buffer;
       if(buff==EOF){ ui->textEdit->append("Файл сообщения слишком мал.");   QThread::sleep(5); exit(0); }
       cnte = 8;
      }
     //выдача очередного бита
     t = buff&1;
     buff >>= 1;
     cnte -= 1;
   Source[i]=t;
     }
}


//ВВОД БИТА сообщения из байта
 int MainWindow:: input_bit()
{     unsigned char t;
   // t=Source[Read_bits];
    //t=(t) ^ (Key[Read_bits]); //Шифр Вернама
      t=Key[Read_bits];
    Read_bits++;

 return t;
}

void write_byte(){
    unsigned char q, cnte=1, bt, bits[9];
    int i;
    char b;
 for(i=0; i<Read_bits; i++){
     bt=Dec[i];
    // bt=(bt) ^ (Key[i]); //Дешифрование
     if(cnte<9) bits[cnte]=bt;
     cnte++;
    if(cnte>8) {
     cnte=1;
     bt=0;
     for(q=8;q>0;q--) bt=bt+pow(2,8-q)*bits[9-q];
     b=bt;
     file_dec.putChar(b);//запись сформированного байта в файл
      }
 }
}

void Read_full(QString str) {
   short i,j,m,n;
   QImage file_empt;
   file_empt.load(str);//загрузка bmp файла по имени из строки str
   m=n=0;//индексы для извлеченного cover
   for(i=0;i<H;++i){
       for(j=0;j<W;++j) {
       Pixels_interp[i][j]=file_empt.pixelIndex(j,i);//запись значения пикселя
       if((i%2==0)&&(j%2==0)) {
           Pixels_pict[m][n]=Pixels_interp[i][j];
           if (n<255) n++; else {m++; n=0;}
           }
        }
     }
}

short int Get_Code(short is, short js){
  unsigned char bit;
  bit=bit_on(8,Pixels_interp[is][js]);
  return bit;
}

void output_bit(unsigned char bit){
    Dec[Read_bits]=bit; Read_bits++;
}

static void Bits_to(unsigned char bit){
    output_bit(bit);
    while(F_bits>0){
        output_bit(!bit);
        F_bits-=1;
    }
}

void Cod_end()
{ F_bits += 1;
  if (low<Q1) Bits_to(0);
  else Bits_to(1);
}

static Tcode value;//текущее состояние кода

void MainWindow::Dec_Go()
{ int  i;
    value=0;
    for( i=1; i<=Cbits; i++){
        value=2*value+input_bit();  }
    low=0;
    high=Top_value;
}

void Cod_go()
{
    low=0;
    high=Top_value;
    F_bits=0;
}

void Input_Sumb(short f, short is, short js)
{
   double Ent;
   int ind_1;
   int P0, P1;
   //область номер f
      Circle[0]=bit_on(BIT-1, Pixels_interp[is][js]);// qDebug()<<"Circle[0]"<<Circle[0];
      Circle[1]=bit_on(BIT, Pixels_interp[is][js]); //qDebug()<<"Circle[1]"<<Circle[1];
      Circle[2]=bit_on(BIT-2, Pixels_interp[is][js]); //qDebug()<<"Circle[2]"<<Circle[2];
      if(is%2==0){ //для четных строк есть опорные точки
      Circle[3]=bit_on(BIT+1, Pixels_interp[is][js-1]); //qDebug()<<"Circle[3]"<<Circle[3];
      Circle[4]=bit_on(BIT+1, Pixels_interp[is][js+1]); //qDebug()<<"Circle[4]"<<Circle[4];
      } else {//для нечетных строк берем предыдущую строку
          if(js%2!=0){//при нечетных столбцах
      Circle[3]=bit_on(BIT+1, Pixels_interp[is-1][js-1]); //qDebug()<<"Circle[3]"<<Circle[3];
      Circle[4]=bit_on(BIT+1, Pixels_interp[is-1][js+1]); //qDebug()<<"Circle[4]"<<Circle[4];
          }
          else{ //при четных столбцах берем верх и низ
              Circle[3]=bit_on(BIT+1, Pixels_interp[is-1][js]); //qDebug()<<"Circle[3]"<<Circle[3];
              Circle[4]=bit_on(BIT+1, Pixels_interp[is+1][js]); //qDebug()<<"Circle[4]"<<Circle[4];
          }

      }
       ind_1=Get_Index();//число из собранных бит
       P0= Prob[f].nbt[0][ind_1]; P1= Prob[f].nbt[1][ind_1];

freq[0]=0; freq[1]=P0; freq[2]=P1;
}

void Born_mod(short f, short is, short js)
{int  i;
   for(i=0; i<NumC; i++) {
        IndexC[i]=i+1;
        Index[i+1]=i;
    }
   Input_Sumb(f,is,js);
   Freq_Cum[NumS]=0;
   for(i=NumS; i>0; i--){ Freq_Cum[i-1]=Freq_Cum[i]+freq[i];}
   if(Freq_Cum[0]>Max_f) abort();
}

//декодирование следующего символа
int MainWindow::Sdec(int Freq_Cum[]){
 long range;
 int cum, symbol;
 range=(long)(high-low)+1;
 cum=(((long)(value-low)+1)*Freq_Cum[0]-1)/range;
 for(symbol=1; Freq_Cum[symbol]>cum; symbol++);
 high=low+(range*Freq_Cum[symbol-1])/Freq_Cum[0]-1;
 low=low+(range*Freq_Cum[symbol])/Freq_Cum[0];
   for(;;) {
      if(high<half) {
      }
      else if(low>=half) {
           value -= half;
           low -= half;
           high -= half;
      }
      else if(low>=Q1 && high<Q3) {
           value -= Q1;
           low -= Q1;
           high -= Q1;
      }
      else break;
      low = 2*low;
      high = 2*high+1;
      value = 2*value+input_bit();
   }
   return symbol;
}

void S_cod(int symbol, int Freq_Cum[])
{
  long range;
  range = (long)(high-low)+1;
  high = low+(range*Freq_Cum[symbol-1])/Freq_Cum[0]-1;
  low = low+(range*Freq_Cum[symbol])/Freq_Cum[0];
  for(;;) {
      if(high<half) {  Bits_to(0);  }
      else if (low>=half) {
           Bits_to(1);
          low -= half;
          high -= half;  }
      else if (low>=Q1 && high<Q3) {
      F_bits +=1;
      low -= Q1;
      high -= Q1; }
      else break;
      low = 2*low;
      high = 2*high+1;
  }
}

void MainWindow::Ar_dec( short f)
{
    int ch, symbol, is,js,p;

for(p=0; p<Sizes_inp[f]; p++){

         is=RegionList_inp[f][p].X;//индекс строки
         js=RegionList_inp[f][p].Y;//индекс столбца
 Born_mod(f,is,js);
 symbol=Sdec(Freq_Cum);
 ch=Index[symbol];
 Mess[Write_bits]=ch; Write_bits++;;
}
AverageEnt=AverageEnt/Sizes_inp[f];
//qDebug()<<"AverageEnt="<<AverageEnt;
AverageEnt=0;

}

//метод лесного пожара
void Fire(int Im[][w], int Height, int Width){
int i,j,k,l, Count_p=0, Kol=0;
int current;
struct Points Point;
Regions=0;
for (i=0; i<Height; i++)
    for(j=0; j<Width; j++) Labels[i][j]=0;

//формирование массива меток
for (j=0; j<Width; j++)
    for(i=0; i<Height; i++){ // в обход крайних пикселей.
        if (Labels[i][j]==0){
           // qDebug()<<"i= "<<i<<" j= "<<j;
            Regions++;//новая область
            Count_p=0;
            Labels[i][j]=Regions;
            Point.X=i;
            Point.Y=j;
            current=bit(Im[i][j]);
            RegionList[Regions][Count_p]=Point;//добавить точку
            Count_p++;
            Sizes[Regions]=Count_p;//размер области
            Stack.push(Point);//поместить в стек точку
            while(!Stack.empty()) { //пока стек не пуст
                Point=Stack.pop();// достать из стека
                for(k=Point.X-1; k<=(Point.X+1);k++)
                    for(l=Point.Y-1; l<=(Point.Y+1); l++){
                       //if( (abs(Im[k][l]-current)<=8) && (Labels[k][l]==0)&&(l>=0)&&(k>=0)&&(l<255)&&(k<255)&&(Sizes[Regions]<Points_Max) ){
                        if( (bit(Im[k][l])==current) && (Labels[k][l]==0)&&(l>=0)&&(k>=0)&&(l<255)&&(k<255)&&(Sizes[Regions]<Points_Max)  ){
                       // if( (Im[k][l]==current) && (Labels[k][l]==0)&&(l>=0)&&(k>=0)&&(l<255)&&(k<255)&&(Sizes[Regions]<Points_Max)  ){
                            Labels[k][l]=Regions;
                            Point.X=k;
                            Point.Y=l;
                           // qDebug()<<"k= "<<k<<" l= "<<l;
                            RegionList[Regions][Count_p]=Point;//добавить точку
                            Count_p++;
                            Sizes[Regions]=Count_p;//размер области
                            Stack.push(Point);//поместить в стек точки
                          } } }
        }
    }

QString st;
QFile file_matrix;
file_matrix.setFileName("D://QT_Progects//Statistic_Fire//matrix.txt");
if (!file_matrix.open(QIODevice::WriteOnly)) {
 qDebug() << "Can not create dec file" << endl;
return;}
QTextStream out(&file_matrix);

for (i=0; i<Height; i++){
    for(j=0; j<Width; j++) st=st+' '+QString::number(Labels[i][j]);
           out << st+"\n";
           st="";
}
file_matrix.close();
/*
qDebug()<<"--Fields--------------------------------------------------------";
st="";
for(i=1; i<Regions; i++) {
    for(j=1; j<=Sizes[i];j++)  qDebug()<<QString::number(Im[RegionList[i][j].X][RegionList[i][j].Y]);
    qDebug()<<"=========================";
}
*/

qDebug()<<"-Sizes---------------------------------------------------------"+QString::number(Regions);
st="";
for(i=1;i<=Regions;i++){  st=st+' '+QString::number(Sizes[i]); Kol+=Sizes[i]; }
qDebug()<<st;
}

void Fire_inp(){//области для интерполированных значений включают три пикселя с правого нижнего угла опорной точки
int Count_p, i,j;
struct Points Point;

for(i=1;i<=Regions;i++){    Count_p=0;
    for(j=0;j<Sizes[i];j++){

//qDebug()<<"Точка "<< RegionList[i][j].X<<" "<<RegionList[i][j].Y;
    Point.X=RegionList[i][j].X*2;
    Point.Y=RegionList[i][j].Y*2+1;
    RegionList_inp[i][Count_p]=Point;//добавить точку 1
    Count_p++;
//qDebug()<<"INP1 "<< Point.X<<" "<<Point.Y;

    Point.X=RegionList[i][j].X*2+1;
    Point.Y=RegionList[i][j].Y*2+1;

    RegionList_inp[i][Count_p]=Point;//добавить точку 2
    Count_p++;
//qDebug()<<"INP2 "<< Point.X<<" "<<Point.Y;

    Point.X=RegionList[i][j].X*2+1;
    Point.Y=RegionList[i][j].Y*2;

    RegionList_inp[i][Count_p]=Point;//добавить точку 3
    Count_p++;
//qDebug()<<"INP3 "<< Point.X<<" "<<Point.Y;
    }
     Sizes_inp[i]=Count_p;//размер новой области
}

//for(i=1;i<Regions;i++) qDebug()<<Sizes_inp[i];
}

void MainWindow::on_Embedding_clicked()
{
    QString str, str1, str_mes;
   int i,j;

    //Цикл обработки картинок
        for(j=1; j<2; j++){
            //Открыть файл с сообщением
            str_mes="D://QT_Progects//Statistic_Fire_auto//mess.txt";;
            file_mess.setFileName(str_mes);
            if (!file_mess.open(QIODevice::ReadOnly)) {
             qDebug() << "Can not create mess file" << endl;
            return;}
            str="D://BOSS_256//"+QString::number(j) +".bmp";
            ui->textEdit->append(str);
            Read_pixels(str);
            Read_bits=0;
            read_mess();//запись бит сообщения в массив

    Fire(Pixels_pict, h, w);//вычисление областей исходной картинки 256х256
    ui->textEdit->append("Количество областей: "+QString::number(Regions));

    ui->textEdit->append("Сбор статистики...");
    for(i=1;i<=Regions;i++){
      if(Sizes[i]>=min){
       Init(i);
       Statistics(i);}
   }
 qDebug()<<"=========================";
    ui->textEdit->append("Интерполирование INP...");
    str1="D://QT_Progects//Statistic_Fire_auto//RS//"+QString::number(j) +"_S.bmp";  //стегоконтейнер
    INP(str1);
    ui->textEdit->append("успешно завершено."+str1);
    Fire_inp();//формируем область в интерполированной картинке


    Dec_Go();
    Write_bits=0;
    //окно прогресса
     QProgressDialog* pp = new QProgressDialog("Построение кода","&Cancel",0,Regions);
     pp->setMinimumDuration(1);
     pp->setWindowTitle("Подождите...");
     pp->setAutoClose(true);
     pp->setAutoReset(true);

    for(i=1; i<=Regions; i++){ //идем по областям
      ui->textEdit->append("Область "+QString::number(i)+", размер "+QString::number(Sizes_inp[i]));
      if(Sizes_inp[i]>=min_inp){

           //демонстрация прогресса
            pp->setValue(i);
            pp->show();
            qApp->processEvents();
            if(pp->wasCanceled()){break;}
          ui->textEdit->append("Арифметическое декодирование...");

          Ar_dec(i);//генерация кода с заданным распределением
          Write_Pix(str1, i);//запись кода в интеролированную картинку
    }
    }
    pp->setValue(Regions);
    delete pp;
 file_mess.close();
  ui->textEdit->append("Контейнер успешно заполнен.");
  float bpp; bpp=Read_bits;
  ui->textEdit->append("Записанных кодовых бит: "+QString::number(Write_bits)+" бит, "+ QString::number(Read_bits)+" битов сообщения, " +QString::number(bpp/Max)+" bpp ");
}

}

void MainWindow::Ar_cod(short f){
   int ch, symbol, p, is, js;;

AverageEnt=0;
for(p=0; p<Sizes_inp[f]; p++){
       is=RegionList_inp[f][p].X;//индекс строки
       js=RegionList_inp[f][p].Y;//индекс столбца

Born_mod(f,is,js);
ch=Get_Code(is,js);
symbol=IndexC[ch];
S_cod(symbol,Freq_Cum);
      }

}

void MainWindow::on_Decoding_clicked()
{ QString str, str3;
    short i,j;
    //обнулим массив пикселей
    for(i=0;i<hei;++i){
         for(j=0;j<wid;++j) Pixels_pict[i][j]=0;}


    //Цикл обработки картинок
        for(j=1; j<11; j++){

            //откроем файл контейнера
            str="D://QT_Progects//Statistic_Fire_auto//RS//"+QString::number(j) +"_S.bmp";//стегоконтейнер

    ui->textEdit->append("Считывание кода...");
    Read_full(str);//считываем матрицу заполненного контейнера

    ui->textEdit->append("Интерполирование INP...");//для получения исходных областей без кода
    str3="D://QT_Progects//Statistic_Fire_auto//inp//"+QString::number(j) +"_inp.bmp"; //пустой интерполированный контейнер для промежуточных расчетов
    INP(str3);
    ui->textEdit->append("успешно завершено.");
    Fire(Pixels_pict, h, w);//вычисление областей исходной картинки 256х256
    ui->textEdit->append("Сбор статистики...");


    for(i=1;i<=Regions;i++){
      if(Sizes[i]>=min){
       Init(i);
       Statistics(i);}
   }

    Fire_inp();//формируем область в интерполированной картинке
    ui->textEdit->append("...завершен.");
    //открываем файл для декодированного сообщения
    file_dec.setFileName("D://QT_Progects//Statistic_Fire_auto//dec//"+QString::number(j) +"_dec.txt");
    if (!file_dec.open(QIODevice::WriteOnly)) {
     qDebug() << "Can not create dec file" << endl;
    return;}

    Write_bits=0; Read_bits=0;
Cod_go();
Read_full(str);//считываем матрицу заполненного контейнера

//окно прогресса
 QProgressDialog* pp = new QProgressDialog("Восстановление кода","&Cancel",0,Regions);
 pp->setMinimumDuration(1);
 pp->setWindowTitle("Подождите...");
 pp->setAutoClose(true);
 pp->setAutoReset(true);

for (i=1; i<=Regions; i++){ //идем по областям
  ui->textEdit->append("Область "+QString::number(i)+", размер "+QString::number(Sizes_inp[i]));
  if(Sizes_inp[i]>=min_inp){
      //демонстрация прогресса
       pp->setValue(i);
       pp->show();
       qApp->processEvents();
       if(pp->wasCanceled()){break;}
      ui->textEdit->append("Арифметическое кодирование...");
      Ar_cod(i);//восстановление кода
}
}
pp->setValue(Regions);
delete pp;
 Cod_end();
 write_byte();
file_dec.close();
ui->textEdit->append("Декодирование завершено. Файл сохранен в каталоге приложения.");
 }
}

void MainWindow::on_Help_clicked()
{
    QTextBrowser *tb = new QTextBrowser(0);
    tb->setOpenExternalLinks(true);
    tb->setHtml("<p>Для встраивания сообщения в контейнер, нажмите кнопку <em>Встроить</em>. Затем выберите контейнер и сообщение.</p>"
               " <p>Формат поддерживаемых контейнеров: 8-битный bmp файл (gray scale) размером 256 на 256 точек. Размер текстового сообщения для встраивания - 25 Кб.</p>"
               " <p>Новый псевдослучайный ключ генерируется в программе при каждом запуске. Поэтому декодирование будет верно, если контейнер был заполнен при том же запуске программы</p>"
               " <p>Для извлечения встроенного сообщения нажмите <em>Извлечь</em> и выберите контейнер S.bmp, который был создан при текущем запуске программы. Декодированное сообщение будет создано в папке с программой. Для просмотра сообщения рекомендуется редактор Notepad++.</p>"
               " <p>&nbsp;</p>");
    tb->resize(400, 250);
    tb->setWindowIcon(QIcon("Help.ico"));
    tb->show();

/* средство для промежуточного тестирования
    short i, j, s;
    unsigned char P, Pixels_bits[H][W];
    QString str, str1;

       QImage file_full(H, W, QImage::Format_Indexed8);
       QVector<QRgb> palette;
       QRgb rgb;
       for (i = 0; i < 256; ++i) { rgb=qRgb(i,i,i); file_full.setColor(i,rgb); } //устанавливается палитра цветов

           //Цикл обработки картинок
           for(s=1; s<11; s++){
               str="D://QT_Progects//Statistic_Fire_auto//RS//"+QString::number(s) +"_S.bmp";
               qDebug()<<str;
               QImage file_empt(H, W, QImage::Format_Indexed8);
              file_empt.load(str);//загрузка bmp файла по имени из строки str

               for(i=0;i<H;++i){
                   for(j=0;j<W;++j) {
                       P=file_empt.pixelIndex(i,j);
                       Pixels_bits[i][j]=bit(P)*255;//запись значения пикселя
                   }}

               str1="D://QT_Progects//Statistic_Fire_auto//bits//"+QString::number(s) +"_.bmp";

               for(i=0;i<H;++i){
                   for(j=0;j<W;++j) {
                      file_full.setPixel(j,i,Pixels_bits[j][i]);//запись пикселя
                   } }

             file_full.save(str1,"BMP");
}
*/
}

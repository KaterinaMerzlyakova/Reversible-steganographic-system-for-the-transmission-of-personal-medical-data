#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT


public:
#define H 512
#define W 512
    explicit MainWindow(QWidget *parent = nullptr);
    int input_bit();
              void Statistics(short);
              void INP(QString);
              void Area_calc(short, short, short);
              void Area_calc1(short, short, short);
              void Ar_dec(short);
              void Ar_cod(short);
              void Dec_Go();
              void read_mess();
              int Sdec(int[]);
    ~MainWindow();


private slots:

    void on_Embedding_clicked();

    void on_Decoding_clicked();

    void on_Help_clicked();



private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H

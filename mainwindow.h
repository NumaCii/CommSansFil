#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnConnect_clicked();
    void on_btnDisconnect_clicked();
    void on_btnSelectionnerCarte_clicked();
    void on_btnMiseAJour_clicked();
    void on_btnPayer_clicked();
    void on_btnCharger_clicked();
    void on_btnQuitter_clicked();

private:
    Ui::MainWindow *ui;

    // État interne
    bool m_connected;
    int  m_solde;

    void updateUI(); // Met à jour l'état des boutons selon connexion
};

#endif // MAINWINDOW_H

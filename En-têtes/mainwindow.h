#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QStatusBar>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    statusBar()->showMessage(tr("Prêt — cliquer sur Connect pour démarrer"));
    refreshUiState();
}

MainWindow::~MainWindow()
{
    if (m_card.isConnected()) m_card.disconnectReader();
    delete ui;
}

// ---------------------------------------------------------------------------
// Slot : Connect (diagramme 2.3.1)
// ---------------------------------------------------------------------------
void MainWindow::on_btnConnect_clicked()
{
    if (m_card.connectReader() != MI_OK) {
        warn(tr("Connexion lecteur impossible : %1").arg(m_card.lastError()));
        return;
    }
    info(tr("Lecteur connecté"));
    refreshUiState();
}

// ---------------------------------------------------------------------------
// Slot : Disconnect (diagramme 2.3.6)
// ---------------------------------------------------------------------------
void MainWindow::on_btnDisconnect_clicked()
{
    m_card.disconnectReader();
    m_cardSelected = false;
    info(tr("Lecteur déconnecté"));
    refreshUiState();
}

// ---------------------------------------------------------------------------
// Slot : Sélectionner la carte (diagramme 2.3.2)
//   poll → lecture identité → lecture compteur
// ---------------------------------------------------------------------------
void MainWindow::on_btnSelectionnerCarte_clicked()

{

    if (m_card.pollCard() != MI_OK) {

        m_card.signalFailure();

        warn(tr("Aucune carte détectée : %1").arg(m_card.lastError()));

        return;

    }

    QString nom, prenom;

    if (m_card.readIdentity(nom, prenom) != MI_OK) {

        m_card.signalFailure();

        warn(tr("Lecture identité KO : %1").arg(m_card.lastError()));

        return;

    }

    ui->lineEditNom->setText(nom);

    ui->lineEditPrenom->setText(prenom);

    int32_t compteur = 0;

    int16_t status = m_card.readCounter(compteur);

    // Si la lecture value échoue, on suppose carte vierge → init silencieuse

    if (status != MI_OK) {

        if (m_card.initValueBlocks() != MI_OK) {

            m_card.signalFailure();

            warn(tr("Init porte-monnaie KO : %1").arg(m_card.lastError()));

            return;

        }

        if (m_card.readCounter(compteur) != MI_OK) {

            m_card.signalFailure();

            warn(tr("Lecture compteur KO après init : %1").arg(m_card.lastError()));

            return;

        }

    }

    ui->lineEditNbUnites->setText(QString::number(compteur));

    m_card.signalSuccess();

    m_cardSelected = true;

    info(tr("Carte sélectionnée — UID %1").arg(m_card.uidHex()));

    refreshUiState();

}



// ---------------------------------------------------------------------------
// Slot : Mise à jour identité (diagramme 2.3.3)
// ---------------------------------------------------------------------------
void MainWindow::on_btnMiseAJour_clicked()
{
    const QString nom    = ui->lineEditNom->text().trimmed();
    const QString prenom = ui->lineEditPrenom->text().trimmed();
    if (nom.isEmpty() || prenom.isEmpty()) {
        warn(tr("Nom et prénom obligatoires"));
        return;
    }

    if (m_card.writeIdentity(nom, prenom) != MI_OK) {
        m_card.signalFailure();
        warn(tr("Écriture identité KO : %1").arg(m_card.lastError()));
        return;
    }

    // Vérification : on relit ce qu'on vient d'écrire
    QString nomRelu, prenomRelu;
    if (m_card.readIdentity(nomRelu, prenomRelu) != MI_OK) {
        warn(tr("Relecture identité KO : %1").arg(m_card.lastError()));
        return;
    }
    if (nomRelu != nom || prenomRelu != prenom) {
        warn(tr("Discordance après écriture (relu : %1 %2)").arg(prenomRelu, nomRelu));
        return;
    }

    m_card.signalSuccess();
    info(tr("Identité mise à jour"));
}

// ---------------------------------------------------------------------------
// Slot : Payer (diagramme 2.3.4)
// ---------------------------------------------------------------------------
void MainWindow::on_btnPayer_clicked()
{
    const int32_t n = ui->spinBoxDecrementer->value();
    if (n <= 0) { warn(tr("Indiquer un nombre d'unités > 0")); return; }

    int32_t solde = 0;
    if (m_card.readCounter(solde) != MI_OK) {
        warn(m_card.lastError()); return;
    }
    if (solde < n) {
        warn(tr("Solde insuffisant (solde = %1, demandé = %2)").arg(solde).arg(n));
        m_card.signalFailure();
        return;
    }

    if (m_card.pay(n) != MI_OK) {
        m_card.signalFailure();
        warn(tr("Paiement KO : %1").arg(m_card.lastError()));
        return;
    }

    // Relecture pour afficher le nouveau solde
    if (m_card.readCounter(solde) == MI_OK) {
        ui->lineEditNbUnites->setText(QString::number(solde));
    }
    m_card.signalSuccess();
    info(tr("Paiement de %1 unités OK — nouveau solde : %2").arg(n).arg(solde));
}

// ---------------------------------------------------------------------------
// Slot : Charger (diagramme 2.3.5)
// ---------------------------------------------------------------------------
void MainWindow::on_btnCharger_clicked()

{

    const int32_t n = ui->spinBoxIncrementer->value();

    if (n <= 0) { warn(tr("Indiquer un nombre d'unités > 0")); return; }

    int32_t avant = 0;

    m_card.readCounter(avant);

    qWarning() << "AVANT charge:" << avant;

    if (m_card.charge(n) != MI_OK) {

        m_card.signalFailure();

        warn(tr("Chargement KO : %1").arg(m_card.lastError()));

        return;

    }

    int32_t apres = 0;

    m_card.readCounter(apres);

    qWarning() << "APRES charge:" << apres;

    ui->lineEditNbUnites->setText(QString::number(apres));

    m_card.signalSuccess();

    info(tr("Rechargement de %1 unités OK — nouveau solde : %2").arg(n).arg(apres));

}

// ---------------------------------------------------------------------------
// État UI : on grise les boutons selon l'état (déconnecté / connecté / carte présente)
// ---------------------------------------------------------------------------
void MainWindow::refreshUiState()
{
    const bool connected = m_card.isConnected();
    ui->btnConnect          ->setEnabled(!connected);
    ui->btnDisconnect       ->setEnabled(connected);
    ui->btnSelectionnerCarte->setEnabled(connected);

    ui->btnMiseAJour ->setEnabled(connected && m_cardSelected);
    ui->btnPayer     ->setEnabled(connected && m_cardSelected);
    ui->btnCharger   ->setEnabled(connected && m_cardSelected);
}

void MainWindow::info(const QString &msg) { statusBar()->showMessage(msg, 5000); }
void MainWindow::warn(const QString &msg)
{
    statusBar()->showMessage(msg, 5000);
    QMessageBox::warning(this, tr("Attention"), msg);
}

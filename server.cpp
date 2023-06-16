#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <iostream>
#include <string>
#include <vector>
#include <map>
using namespace std;

#define KULLANICI_MAX 2;
int maxKullanici = KULLANICI_MAX;

string komutlarString[8] = {"login", "logout", "list rooms", "list users", "enter", "whereami", "open", "close"};

struct MesajDuyurmaBilgisi
{
    vector<int> *soketNumaralari;
    string *mesaj;
};

void ExitWithMesssage(string Msg)
{
    cout << Msg;
    exit(EXIT_FAILURE);
}

void *MesajiDuyur(void *arg);
void *KullaniciYakalayan(void *arg);
void *KullaniciDinleyen(void *arg);
void *MesajiDuyur(vector<int> soketNumaralari, string mesaj);
string KomutGetir(string value)
{
    int komutSayisi = sizeof(komutlarString) / sizeof(komutlarString[0]);
    for (int i = 0; i < komutSayisi; i++)
        if (value.rfind(komutlarString[i], 0) == 0)
            return komutlarString[i];
    return "";
}

class Kullanici{
    public:
        string KullaniciAdi;
        int SoketID;
        int OdaID;
        Kullanici(){ KullaniciAdi = ""; SoketID = -1; OdaID = 0; }
        Kullanici(string ad, int soket, int oda){
            this->KullaniciAdi = ad;
            this->SoketID = soket;
            this->OdaID = oda;
        }
};

class Oda
{
public:
    string OdaSahibi;
    string OdaAdi;
    vector<Kullanici> KullaniciListesi;
    void KullaniciEkle(Kullanici k){
        for(int i = 0; i < KullaniciListesi.size(); i++)
            if(KullaniciListesi[i].SoketID == k.SoketID)
                return;
        
        KullaniciListesi.push_back(k);
    }

    void KullaniciCikar(Kullanici k){
        this->KullaniciCikar(k.SoketID);
    }

    void KullaniciCikar(int soketID){
        for (int i = 0; i < KullaniciListesi.size(); ++i)
            if (KullaniciListesi[i].SoketID == soketID)
            {
                KullaniciListesi.erase(KullaniciListesi.begin() + i);
                break;
            }
    }

};

class Odalar
{
public:
    vector<Oda> odalar;
    int soket;

    int OdaEkle(Oda oda)
    {
        this->odalar.push_back(oda);
        return this->odalar.size() - 1;
    }

    bool KullaniciEkle(Kullanici k)
    {
        if (k.OdaID < odalar.size() && k.OdaID > -1)
        {
            odalar[k.OdaID].KullaniciListesi.push_back(k);
            return true;
        }
        return false;
    }

    void SoketAyarla(int soket)
    {
        this->soket = soket;
    }

    void KullaniciCikar(Kullanici k)
    {
        if (k.OdaID < odalar.size() && k.OdaID > -1){
            odalar[k.OdaID].KullaniciCikar(k);
            this->DigerlerineHaberVer(odalar[k.OdaID].KullaniciListesi, k.KullaniciAdi, odalar[k.OdaID].OdaAdi);
            
            bool odaSahibiMi = odalar[k.OdaID].OdaSahibi == k.KullaniciAdi;
            if (odaSahibiMi)
                this->OdayiKapat(k.OdaID);

        }
    }

    void OdayiKapat(int odaID)
    {
        if(odaID == 0) return;
        for (int i = 0; i < odalar[odaID].KullaniciListesi.size(); ++i)
        {
            odalar[odaID].KullaniciListesi[i].OdaID = 0;
            odalar[0].KullaniciEkle(odalar[odaID].KullaniciListesi[i]);
        }
        odalar.erase(odalar.begin() + odaID);
    }

    void DigerlerineHaberVer(vector<Kullanici> liste, string cikan, string odaAdi)
    {
        char metin[1024];
        string smetin = (cikan) + " user ('" + odaAdi + "') left.";
        strcpy(metin, smetin.c_str());

        for (int j = 0; j < liste.size(); j++)
            send(liste[j].SoketID, metin, strlen(metin), 0);
    }

    int OdaIDGetir(string ad){
        for(int i= 0; i < odalar.size(); i++)
            if(odalar[i].OdaAdi == ad)
                return i;
        return -1;
    }

    bool OdaVarMi(string ad){
        for(int i = 0; i < odalar.size(); i++)
            if(odalar[i].OdaAdi == ad)
                return true;
        return false;
    }

    void KullaniciOdaDegistir(Kullanici k, int odaID, int eskiOdaID){
        if(eskiOdaID < odalar.size()){
            cout << "kullanıcı " << odaID << " dan çıktı." << endl;
            odalar[eskiOdaID].KullaniciCikar(k.SoketID);
        }
        if(odaID < odalar.size()){
            cout << "kullanıcı " << odaID << " ye eklendi" << endl;
            odalar[odaID].KullaniciEkle(k);
        }
    }
};

class Sunucu
{
private:
    int ServerPort = 9190;
    int ServerSoket;
    int gelenGiden = 0;
    struct sockaddr_in ServerAddr;
    socklen_t ServerAddrLen = sizeof(ServerAddr);

    Odalar odalar;
    vector<Kullanici> kullanicilar;

    void SoketBaslat()
    {
        if ((this->ServerSoket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
            ExitWithMesssage("Error, server socket down.");

        this->ServerAddr.sin_family = AF_INET;
        this->ServerAddr.sin_addr.s_addr = INADDR_ANY;
        this->ServerAddr.sin_port = htons(ServerPort);
    }

    void Bind()
    {
        if (bind(this->ServerSoket, (struct sockaddr *)&this->ServerAddr, sizeof(this->ServerAddr)) < 0)
            ExitWithMesssage("Error, bind failed.");
    }

    void Dinle()
    {
        if (listen(this->ServerSoket, maxKullanici) < 0)
            ExitWithMesssage("Error, listen failed.");

        this->KullaniciYakala();
    }

    bool KullaniciEklenebilirMi()
    {
        return kullanicilar.size() < maxKullanici;
    }

    void KullaniciYakala()
    {
        int istemci;
        while ((istemci = accept(this->ServerSoket, (struct sockaddr *)&this->ServerAddr, (socklen_t *)&ServerAddrLen)))
        {
            this->gelenGiden++;

            // aktif olarak bağlı kullanıcı sayısına bakıyor.
            if (!this->KullaniciEklenebilirMi())
            {
                char red[12] = "Server full";
                send(istemci, red, strlen(red), 0);
                close(istemci);
                continue;
            }

            pair<Sunucu *, int *> bilgi(this, &istemci);
            pthread_t pIstemci;
            pthread_create(&pIstemci, NULL, KullaniciYakalayan, static_cast<void *>(&bilgi));
        }
    }

    void KomutCalistir(int soketID, string komut, string veri)
    {
        string mesaj = "";
        if (komut == "logout")
            this->KullaniciCikti(soketID);
        else if(komut == "list rooms"){
            for(int i = 0; i < this->odalar.odalar.size(); i++)
                mesaj += "["+ to_string(i) +"] " + this->odalar.odalar[i].OdaAdi + " " + this->odalar.odalar[i].OdaSahibi;
            send(soketID, mesaj.c_str(), strlen(mesaj.c_str()), 0);
        }else if(komut == "list users"){
            int OdaID = 0;
            for(int i = 0; i < kullanicilar.size(); i++){
                if(kullanicilar[i].SoketID == soketID){
                    OdaID = kullanicilar[i].OdaID; break;
                }
            }
            if(OdaID < this->odalar.odalar.size())
                for(int i = 0; i < this->odalar.odalar[OdaID].KullaniciListesi.size(); i++)
                    mesaj += "["+ to_string(i) +"] " + this->odalar.odalar[OdaID].KullaniciListesi[i].KullaniciAdi;
            send(soketID, mesaj.c_str(), strlen(mesaj.c_str()), 0);
        }else if(komut == "whereami"){
            int odaID = this->kullaniciGetir(soketID).OdaID;
            mesaj = (odaID < this->odalar.odalar.size()) ? "Oda adı : " + this->odalar.odalar[odaID].OdaAdi + ", Oda Sahibi : " + this->odalar.odalar[odaID].OdaSahibi + ", Aktif kullanici sayisi: " + to_string(this->odalar.odalar[odaID].KullaniciListesi.size()) : "";
            send(soketID, mesaj.c_str(), strlen(mesaj.c_str()), 0);
        }else if(komut == "enter"){
            if(veri.length() < 6) return;

            string odaAdi = veri.substr(6);
            int odaID = this->odalar.OdaIDGetir(odaAdi);
            if(odaID < 0){
                mesaj = "There is no such room";
                send(soketID, mesaj.c_str(), mesaj.length(), 0);
                return;
            }

            for(int i = 0; i < kullanicilar.size(); i++)
                if(kullanicilar[i].SoketID == soketID)
                    KullaniciOdaDegistir(soketID, odaID, kullanicilar[i].OdaID);

        }else if(komut == "open"){
            if(veri.length() < 5) return;

            string odaAdi = veri.substr(5);
            if(odalar.OdaVarMi(odaAdi)){
                mesaj = "Böyle bir oda zaten var.";
                send(soketID, mesaj.c_str(), mesaj.length(), 0);
                return;
            }

            Kullanici k = kullaniciGetir(soketID);
            Oda* odam = new Oda();
            odam->OdaAdi = odaAdi;
            odam->OdaSahibi = k.KullaniciAdi;
            odam->KullaniciEkle(k);
            int odaID = odalar.OdaEkle(*odam);

            for(int i = 0; i < kullanicilar.size(); i++)
                if(kullanicilar[i].SoketID == soketID)
                    KullaniciOdaDegistir(soketID, odaID, kullanicilar[i].OdaID);
            
            mesaj = "Successfully created room.";
            send(soketID, mesaj.c_str(), mesaj.length(), 0);
        }else if(komut == "close"){
            if(veri.length() < 6) return;
            string odaAdi = veri.substr(6);
            if(!odalar.OdaVarMi(odaAdi)){
                mesaj = "There is no such room.";
                send(soketID, mesaj.c_str(), mesaj.length(), 0);
                return;
            }
            OdaKapat(soketID, odaAdi);
        }
    }

public:
    void Baslat()
    {
        Oda *lobi = new Oda();
        lobi->OdaSahibi = "";
        lobi->OdaAdi = "Lobby";
        this->odalar.OdaEkle(*lobi);

        this->SoketBaslat();
        this->Bind();
        this->odalar.SoketAyarla(this->ServerSoket);
        this->Dinle();
    }

    bool KullaniciVarMi(string kullaniciAdi)
    {
        for (int i = 0; i < kullanicilar.size(); i++)
            if (kullanicilar[i].KullaniciAdi == kullaniciAdi)
                return true;
        return false;
    }

    void KullaniciGiris(int soket, string KullaniciAdi)
    {
        Kullanici* k = new Kullanici(KullaniciAdi, soket, 0);
        this->kullanicilar.push_back(*k);
        this->odalar.KullaniciEkle(*k);
    }

    void KullaniciCikti(int cikanKullanicininIstemcisi)
    {
        cout << "Çıkmak isteyen: " << cikanKullanicininIstemcisi << endl;
        Kullanici *k = new Kullanici();
        int index = -1;

        for(int i = 0; i < kullanicilar.size(); i++){
            if(kullanicilar[i].SoketID == cikanKullanicininIstemcisi){
                k->KullaniciAdi = kullanicilar[i].KullaniciAdi;
                k->SoketID = kullanicilar[i].SoketID;
                k->OdaID = kullanicilar[i].OdaID;
                index = i;
                break;
            }
        }

        if (k->SoketID != -1){
            this->KullaniciCikar(*k);
            this->kullanicilar.erase(
                this->kullanicilar.begin() + index
            );
        }else{
            cout << "socket -1" << endl;
            return;
        }
        
        for (auto i = this->kullanicilar.begin(); i != this->kullanicilar.end(); i++)
            cout << i->KullaniciAdi << endl;

        string finalMsg = ".";
        send(k->SoketID, finalMsg.c_str(), 1, 0);

        close(k->SoketID);
    }

    void KullaniciCikar(Kullanici k)
    {
        for (auto i = this->kullanicilar.begin(); i != this->kullanicilar.end();)
        {
            if (((i)->SoketID == k.SoketID)){
                this->odalar.KullaniciCikar(k);
                if(this->odalar.odalar[k.OdaID].OdaSahibi == k.KullaniciAdi){
                    for(int i= 0; i < this->kullanicilar.size(); i++)
                        if(this->kullanicilar[i].OdaID == k.OdaID)
                            this->kullanicilar[i].OdaID = 0;
                }
                break;
            }else
                i++;
        }
    }

    Kullanici kullaniciGetir(int soket){
        Kullanici *k = new Kullanici("", -1, -1);
        for(int i = 0; i < kullanicilar.size(); i++)
            if(kullanicilar[i].SoketID == soket){
                k->KullaniciAdi = kullanicilar[i].KullaniciAdi;
                k->SoketID = kullanicilar[i].SoketID;
                k->OdaID = kullanicilar[i].OdaID;
            }
        return *k;
    }

    void MesajDuyur(int soket, string mesaj)
    {
        Kullanici k = this->kullaniciGetir(soket);
        if (k.SoketID == -1) return;

        if (k.OdaID <= this->odalar.odalar.size())
        {
            vector<int> butunSoketler;
            for (int i = 0; i < this->odalar.odalar[k.OdaID].KullaniciListesi.size(); i++)
            {
                int s = this->odalar.odalar[k.OdaID].KullaniciListesi[i].SoketID;
                if (s != soket)
                    butunSoketler.push_back(s);
            }

            mesaj = "[Oda:" + to_string(k.OdaID) + "] " + k.KullaniciAdi + " : " + mesaj;

            char msg[1024] = {0};
            strcpy(msg, mesaj.c_str());
            for (int i = 0; i < butunSoketler.size(); i++)
                send(butunSoketler[i], (msg), strlen(msg), 0);
        }
    }

    void KomutIsle(int soketID, string veri)
    {
        for (int i = 0; i < sizeof(komutlarString) / sizeof(komutlarString[0]); i++)
        {
            if (veri == komutlarString[i])
            {
                this->KomutCalistir(soketID, komutlarString[i], veri);
                break;
            }
            else if ( veri.find(komutlarString[i] + " ") == 0 )
            {
                this->KomutCalistir(soketID, komutlarString[i], veri);
                break;
            }
        }
    }

    void KullaniciOdaDegistir(int soketID, int odaID, int eskiOdaID){
        
        for(int i = 0; i < kullanicilar.size(); i++){
            if(kullanicilar[i].SoketID == soketID){
                odalar.KullaniciOdaDegistir(kullanicilar[i], odaID, kullanicilar[i].OdaID);
                kullanicilar[i].OdaID = odaID;
            }
        }

    }

    void OdaKapat(int soketID, string odaAdi){
        for(int i = 0; i < odalar.odalar.size(); i++){
            if(
                odalar.odalar[i].OdaAdi == odaAdi
                &&
                odalar.odalar[i].OdaSahibi == kullaniciGetir(soketID).KullaniciAdi
            ){
                int odaID = i;

                for(int j = 0; j < odalar.odalar[i].KullaniciListesi.size(); j++)
                    KullaniciOdaDegistir(odalar.odalar[i].KullaniciListesi[j].SoketID, 0, i);
                    
                for(int j = 0; j < kullanicilar.size(); j++)
                    if(kullanicilar[i].OdaID == odaID)
                        KullaniciOdaDegistir(kullanicilar[i].SoketID, 0, odaID);

                odalar.OdayiKapat(i);
            }
        }
    }
};

void *KullaniciYakalayan(void *arg)
{
    pair<Sunucu *, int *> *bilgi = static_cast<pair<Sunucu *, int *> *>(arg);
    int istemci = *bilgi->second;
    pair<Sunucu *, int *> altThreadlereBilgi(bilgi->first, &istemci);
    bool loged = false;
    char mesajLogin[1024] = {0};
    while (!loged)
    {
        memset(mesajLogin, 0, sizeof(mesajLogin));
        int oku = read(istemci, mesajLogin, 1024);
        if (oku > 0)
        {
            if (string(mesajLogin).find("login ") == 0)
            {
                string kullaniciAdi = string(mesajLogin);
                kullaniciAdi = kullaniciAdi.substr(6);
                if (bilgi->first->KullaniciVarMi(kullaniciAdi) == true)
                {
                    string Hata = "This username is being used.";
                    send(istemci, Hata.c_str(), Hata.length(), 0);
                    continue;
                }
                else
                {
                    loged = true;
                    bilgi->first->KullaniciGiris(istemci, kullaniciAdi);
                }
                break;
            }
        }
        else if (oku == 0)
        {
            break;
        }
    }

    if (!loged)
        return NULL;

    pthread_t dinleyici;
    pthread_create(&dinleyici, NULL, KullaniciDinleyen, static_cast<void *>(&altThreadlereBilgi));
    pthread_join(dinleyici, NULL);

    return NULL;
}

void *KullaniciDinleyen(void *arg)
{
    pair<Sunucu *, int *> *bilgi = static_cast<pair<Sunucu *, int *> *>(arg);
    int *istemciSoketi = (bilgi->second);
    cout << *istemciSoketi << endl;
    char gelenMesaj[1024] = {0};
    bool userWhile = true;
    while (userWhile)
    {
        memset(gelenMesaj, 0, sizeof(gelenMesaj));
        int oku = read(*istemciSoketi, gelenMesaj, 1024);
        if (oku > 0)
        {
            string kontrol = KomutGetir(string(gelenMesaj));
            if (!kontrol.empty())
            {
                bilgi->first->KomutIsle(*istemciSoketi, gelenMesaj);
            }
            else
            {
                bilgi->first->MesajDuyur(*istemciSoketi, gelenMesaj);
                cout << "[" << *istemciSoketi << "] Message = " << gelenMesaj << endl;
            }
        }
        else if (oku == 0 || oku < 0)
        {
            userWhile = false;
        }
    }
    bilgi->first->KullaniciCikti(*istemciSoketi);
    return NULL;
}

int main()
{
    Sunucu *sunucu = new Sunucu();
    sunucu->Baslat();
    return 0;
}

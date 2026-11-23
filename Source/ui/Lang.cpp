#include "Lang.h"

#include <atomic>

namespace lang
{

namespace
{
std::atomic<Language> chosen { Language::italiano };

struct Row { const char* it; const char* en; const char* fr; };

// Le tre colonne: italiano (la chiave), inglese, francese.
// Il francese e' in maiuscolo senza accenti, come la serigrafia dei pannelli.
const Row table[] =
{
    // le pagine sopra il display
    { "CAMPIONA",     "SAMPLE",       "ECHANTILLON" },
    { "FETTE",        "CHOP",         "DECOUPE" },
    { "SUONO",        "SOUND",        "SON" },
    { "SEQUENZA",     "SEQUENCE",     "SEQUENCE" },
    { "BATTUTE",      "BARS",         "MESURES" },
    { "BRANO",        "SONG",         "MORCEAU" },
    { "MIX",          "MIX",          "MIX" },
    { "DISCO",        "DISK",         "DISQUE" },

    // trasporto e registrazione
    { "PLAY",         "PLAY",         "LECTURE" },
    { "STOP",         "STOP",         "ARRET" },
    { "REC SEQ",      "SEQ REC",      "ENR SEQ" },
    { "SOVRAINCIDI",  "OVERDUB",      "SUPERPOSER" },
    { "REGISTRA",     "RECORD",       "ENREGISTRER" },
    { "NUOVA SEQ",    "NEW SEQ",      "NOUV SEQ" },
    { "COPIA",        "COPY",         "COPIER" },

    // modifica
    { "CANCELLA",     "ERASE",        "EFFACER" },
    { "ANNULLA",      "UNDO",         "ANNULER" },
    { "CORREGGI",     "TIMING",       "CORRIGER" },
    { "RADDOPPIA",    "DOUBLE",       "DOUBLER" },
    { "RICAMPIONA",   "RESAMPLE",     "RECHANTILL" },
    { "RIFAI",        "REDO",         "REFAIRE" },

    // come suonano i pad
    { "NOTE REPEAT",  "NOTE REPEAT",  "REPETITION" },
    { "FULL LEVEL",   "FULL LEVEL",   "NIVEAU MAX" },
    { "16 LIVELLI",   "16 LEVELS",    "16 NIVEAUX" },
    { "MUTE",         "MUTE",         "COUPER" },
    { "SOLO",         "SOLO",         "SOLO" },
    { "SCEGLI",       "SELECT",       "CHOISIR" },
    { "SHIFT",        "SHIFT",        "SHIFT" },
    { "DISPOSIZIONE", "LAYOUT",       "DISPOSITION" },
    { "SUONA",        "PLAY MODE",    "MODE JEU" },
    { "SEQUENZE",     "SEQUENCES",    "SEQUENCES" },
    { "ACCORDA SU",   "TUNE UP",      "ACCORD +" },
    { "ACCORDA GIU",  "TUNE DOWN",    "ACCORD -" },

    // spostarsi
    { "SU",           "UP",           "HAUT" },
    { "GIU",          "DOWN",         "BAS" },
    { "SINISTRA",     "LEFT",         "GAUCHE" },
    { "DESTRA",       "RIGHT",        "DROITE" },
    { "BATTUTA PREC", "PREV BAR",     "MES PREC" },
    { "BATTUTA SUCC", "NEXT BAR",     "MES SUIV" },

    // tempo e servizio
    { "TAP",          "TAP",          "TAP" },
    { "METRO",        "METRO",        "METRO" },
    { "SETUP",        "SETUP",        "REGLAGES" },
    { "A LIVE",       "TO LIVE",      "VERS LIVE" },
    { "GENERALE",     "MASTER",       "GENERAL" },
    { "PARAMETRI",    "PAD PARAMS",   "PARAMETRES" },
    { "MIXER",        "DRUM MIX",     "MIXAGE" },
    { "SFOGLIA",      "BROWSE",       "PARCOURIR" },
    { "AGG EFFETTO",  "ADD FX",       "AJ EFFET" },
    { "AGG TRACCIA",  "ADD TRACK",    "AJ PISTE" },
    { "PASSO",        "STEP",         "PAS" },
    { "TEMPO",        "TEMPO",        "TEMPO" },
    { "SWING",        "SWING",        "SWING" },
    { "VOLUME",       "VOLUME",       "VOLUME" },

    // i tasti sotto il display
    { "SPACCA",       "CHOP",         "DECOUPER" },
    { "PULISCI AI",   "CLEAN AI",     "NETTOYER AI" },
    { "PULISCI MP3",  "CLEAN MP3",    "NETTOYER MP3" },
    { "RIEMPI",       "FILL",         "REMPLIR" },
    { "FETTE +",      "SLICES +",     "TRANCHES +" },
    { "FETTE -",      "SLICES -",     "TRANCHES -" },
    { "ARMA",         "ARM",          "ARMER" },
    { "SOGLIA +",     "THRESH +",     "SEUIL +" },
    { "SOGLIA -",     "THRESH -",     "SEUIL -" },
    { "ASCOLTA",      "MONITOR",      "ECOUTE" },
    { "SALVA SCENA",  "SAVE SCENE",   "ENR SCENE" },
    { "APRI SCENA",   "OPEN SCENE",   "OUVRIR SCENE" },
    { "IN LIBRERIA",  "TO LIBRARY",   "VERS BIBLIO" },
    { "LIBRERIA",     "LIBRARY",      "BIBLIOTHEQUE" },

    // le righe del display
    { "PAD",          "PAD",          "PAD" },
    { "LIVELLO",      "LEVEL",        "NIVEAU" },
    { "ACCORDA",      "TUNE",         "ACCORD" },
    { "STATO",        "STATE",        "ETAT" },
    { "SOGLIA",       "THRESHOLD",    "SEUIL" },
    { "DURATA",       "LENGTH",       "DUREE" },
    { "ANTICIPO",     "PRE-REC",      "PRE-ENR" },
    { "INGRESSO",     "INPUT",        "ENTREE" },

    // messaggi
    { "PRONTO",       "READY",        "PRET" },
    { "VUOTO",        "EMPTY",        "VIDE" },
    { "PAD VUOTO",    "EMPTY PAD",    "PAD VIDE" },
    { "NIENTE DA ANNULLARE", "NOTHING TO UNDO", "RIEN A ANNULER" },
    { "NIENTE DA RIFARE",    "NOTHING TO REDO", "RIEN A REFAIRE" },
    { "BANCHI A-D",   "BANKS A-D",    "BANQUES A-D" },
    { "BANCHI E-H",   "BANKS E-H",    "BANQUES E-H" },
    { "FETTE SUI PAD", "SLICES ON PADS", "TRANCHES SUR PADS" },
    { "SPACCO IL BRANO", "CHOPPING",  "JE DECOUPE" },
    { "ANALIZZO IL BRANO", "ANALYSING", "J'ANALYSE" },
    { "NIENTE DA SPACCARE", "NOTHING TO CHOP", "RIEN A DECOUPER" },
    { "NON LEGGO QUEL FILE", "CANNOT READ FILE", "FICHIER ILLISIBLE" },
    { "NON HO TROVATO FETTE", "NO SLICES FOUND", "AUCUNE TRANCHE" },
    { "PULISCO",      "CLEANING",     "JE NETTOIE" },
    { "PULITO",       "CLEANED",      "NETTOYE" },
    { "ARMATO",       "ARMED",        "ARME" },
    { "REGISTRO",     "RECORDING",    "J'ENREGISTRE" },
    { "REGISTRATO",   "RECORDED",     "ENREGISTRE" },
    { "NIENTE INGRESSO", "NO INPUT",  "PAS D'ENTREE" },
    { "ASPETTO IL SUONO", "WAITING FOR SOUND", "J'ATTENDS LE SON" },
    { "SCENA SALVATA", "SCENE SAVED", "SCENE ENREGISTREE" },
    { "SCENA APERTA", "SCENE OPENED", "SCENE OUVERTE" },
    { "SALVATO IN LIBRERIA", "SAVED TO LIBRARY", "ENR DANS BIBLIO" },
    { "NON CI RIESCO", "FAILED",      "ECHEC" },
    { "SUONI",        "SOUNDS",       "SONS" },
    { "ACCESO",       "ON",           "ACTIF" },
    { "SPENTO",       "OFF",          "INACTIF" },
    { "MUTO",         "MUTED",        "COUPE" },
    { "CANCELLATO",   "ERASED",       "EFFACE" },
    { "COPIATO SU",   "COPIED TO",    "COPIE SUR" },
    { "ANNULLATO",    "UNDONE",       "ANNULE" },
    { "RIFATTO",      "RIDONE",       "REFAIT" },
    { "TUTTI SUONANO", "ALL UNMUTED", "TOUS ACTIFS" },
    { "BATTI IL PAD", "HIT THE PAD",  "FRAPPE LE PAD" },
    { "SCEGLI I PAD", "PICK THE PADS", "CHOISIS LES PADS" },
    { "FERMO",        "STOPPED",      "ARRETE" },
    { "SCEGLI SENZA SUONARE", "SELECT WITHOUT PLAYING", "CHOISIR SANS JOUER" },
    { "PANORAMA",     "PAN",          "PANORAMIQUE" },
    { "INIZIO",       "START",        "DEBUT" },
    { "FINE",         "END",          "FIN" },
    { "NESSUN GRUPPO", "NO GROUP",    "AUCUN GROUPE" },
    { "GRUPPO",       "GROUP",        "GROUPE" },
    { "ROVESCIO",     "REVERSE",      "INVERSE" },
    { "DRITTO",       "FORWARD",      "NORMAL" },
    { "METRONOMO",    "METRONOME",    "METRONOME" },
    { "SOLO QUESTO",  "SOLO",         "SOLO" },
    { "POSIZIONE",    "POSITION",     "POSITION" },
    { "COLPI",        "HITS",         "COUPS" },
    { "CANCELLA SEQ", "CLEAR SEQ",    "EFFACER SEQ" },
    { "DIMEZZA",      "HALVE",        "DIVISER" },
    { "COPIA SEQ",    "COPY SEQ",     "COPIER SEQ" },
    { "SEQ",          "SEQ",          "SEQ" },
    { "FAMIGLIA",     "FAMILY",       "FAMILLE" },
    { "SUONA E REGISTRA", "PLAY AND RECORD", "JOUER ET ENREGISTRER" },
    { "SUONA SENZA REGISTRARE", "PLAY WITHOUT RECORDING", "JOUER SANS ENREGISTRER" },
    { "BATTERIA",     "DRUMS",        "BATTERIE" },
    { "PERCUSSIONI",  "PERCUSSION",   "PERCUSSIONS" },
    { "VOCE",         "VOICE",        "VOIX" },
    { "GIRI",         "LOOPS",        "BOUCLES" },
    { "ACCORDI",      "CHORDS",       "ACCORDS" },
    { "BASSO",        "BASS",         "BASSE" },
    { "REGISTRATO",   "RECORDED",     "ENREGISTRE" },

    // tasti della finestra
    { "AUDIO",        "AUDIO",        "AUDIO" },
    { "CARICA",       "LOAD",         "CHARGER" },
    { "PULISCI",      "CLEAN",        "NETTOYER" },
    { "BANCHI",       "BANKS",        "BANQUES" },
    { "SCENA",        "SCENE",        "SCENE" },
    { "PUSH 2 NON COLLEGATO", "PUSH 2 NOT CONNECTED", "PUSH 2 NON CONNECTE" },
    { "NESSUNA SCHEDA AUDIO", "NO AUDIO DEVICE", "AUCUNE CARTE AUDIO" },
    { "VOCI",         "VOICES",       "VOIX" },
    { "PORTA USER",   "USER PORT",    "PORT USER" },
    { "PORTA LIVE",   "LIVE PORT",    "PORT LIVE" }
};
}

void set (Language l)
{
    chosen.store (l);
}

Language current()
{
    return chosen.load();
}

juce::String name (Language l)
{
    switch (l)
    {
        case Language::english:  return "UK";
        case Language::francais: return "FR";
        default:                 return "ITA";
    }
}

juce::String nextName()
{
    switch (current())
    {
        case Language::italiano: return name (Language::english);
        case Language::english:  return name (Language::francais);
        default:                 return name (Language::italiano);
    }
}

juce::String t (const juce::String& italian)
{
    const auto l = current();
    if (l == Language::italiano || italian.isEmpty())
        return italian;

    for (const auto& row : table)
        if (italian == row.it)
            return l == Language::english ? juce::String (row.en) : juce::String (row.fr);

    return italian;
}

juce::String phrase (const juce::String& text)
{
    if (current() == Language::italiano || text.isEmpty())
        return text;

    auto words = juce::StringArray::fromTokens (text, " ", "");
    juce::String out;

    int i = 0;
    while (i < words.size())
    {
        // si prova prima col pezzo piu' lungo, poi si accorcia
        int taken = 0;
        juce::String translated;

        for (int n = juce::jmin (4, words.size() - i); n >= 1; --n)
        {
            juce::String candidate;
            for (int k = 0; k < n; ++k)
                candidate += (k > 0 ? " " : "") + words[i + k];

            const auto word = t (candidate);
            if (word != candidate)
            {
                translated = word;
                taken = n;
                break;
            }
        }

        if (taken == 0)
        {
            translated = words[i];
            taken = 1;
        }

        out += (out.isEmpty() ? "" : " ") + translated;
        i += taken;
    }

    return out;
}

juce::String t (const juce::String& italian, const juce::String& tail)
{
    const auto word = t (italian);
    return tail.isEmpty() ? word : word + " " + tail;
}

} // namespace lang

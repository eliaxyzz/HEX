/**
 * @file localization.cpp
 * @brief Translation table and lookup.
 */

#include "core/localization.h"

namespace hexui {

    namespace {
        /** @brief One table row: a key and its two translations. */
        struct Entry {
            StringKey key;
            const char* it;
            const char* en;
        };

        /**
         * @brief The phrases, one row per key.
         * @note Repeating the key next to its translations is redundant on purpose:
         * it makes "row i holds key i" checkable. Without it, an insertion in the
         * middle would silently shift every later phrase.
         */
        constexpr std::array<Entry, STRING_COUNT> TABLE{{
            // --- Shared ---
            {StringKey::COLOUR_RED,            "Rosso",              "Red"},
            {StringKey::COLOUR_BLUE,           "Blu",                "Blue"},
            {StringKey::COLOUR_RED_UPPER,      "ROSSO",              "RED"},
            {StringKey::COLOUR_BLUE_UPPER,     "BLU",                "BLUE"},
            {StringKey::BACK,                  "Indietro",           "Back"},

            // --- Default names ---
            {StringKey::NAME_HUMAN,            "Umano",              "Human"},
            {StringKey::NAME_HUMAN_2,          "Umano 2",            "Human 2"},
            {StringKey::NAME_COMPUTER,         "Computer",           "Computer"},
            {StringKey::NAME_GUEST,            "Ospite",             "Guest"},

            // --- Main menu ---
            {StringKey::MENU_SUBTITLE,
             "Rosso collega alto e basso  -  Blu collega sinistra e destra",
             "Red connects top and bottom  -  Blue connects left and right"},
            {StringKey::MENU_MODE_HUMAN_COMPUTER, "Umano vs Computer", "Human vs Computer"},
            {StringKey::MENU_MODE_HUMAN_HUMAN, "Umano vs Umano",     "Human vs Human"},
            {StringKey::MENU_VARIANT_NORMAL,   "Normale",            "Normal"},
            {StringKey::MENU_VARIANT_ARCADE,   "Arcade",             "Arcade"},
            {StringKey::MENU_DIFFICULTY_EASY,  "Facile",             "Easy"},
            {StringKey::MENU_DIFFICULTY_MEDIUM,"Medio",              "Medium"},
            {StringKey::MENU_DIFFICULTY_HARD,  "Difficile",          "Hard"},
            {StringKey::MENU_NAME_1,           "Nome giocatore 1 ",
                                               "Player 1 name "},
            {StringKey::MENU_NAME_2,           "Nome giocatore 2 ",
                                               "Player 2 name "},
            {StringKey::MENU_PLAY_AS_RED,      "Rosso",              "Red"},
            {StringKey::MENU_PLAY_AS_BLUE,     "Blu",                "Blue"},
            {StringKey::MENU_PLAY_AS_RANDOM,   "Casuale",            "Random"},
            {StringKey::MENU_PLAY,             "GIOCA",              "PLAY"},
            {StringKey::MENU_LOAD,             "Carica partita",     "Load game"},
            {StringKey::MENU_ONLINE,           "Gioca Online",       "Play Online"},
            {StringKey::MENU_SETTINGS,         "Impostazioni",       "Settings"},
            {StringKey::MENU_QUIT,             "Esci",               "Quit"},
            {StringKey::MENU_LOAD_TITLE,       "Carica partita",     "Load game"},
            {StringKey::MENU_LOAD_EMPTY,       "Nessuna partita salvata",
                                               "No saved games"},
            {StringKey::MENU_LOAD_HINT,
             "Scegli un salvataggio  -  ESC per annullare",
             "Pick a saved game  -  ESC to cancel"},
            {StringKey::MENU_LOAD_PREV,        "Precedenti",         "Previous"},
            {StringKey::MENU_LOAD_NEXT,        "Successivi",         "Next"},

            // --- Save slot management ---
            {StringKey::SAVE_DELETE_PROMPT,
             "Eliminare questo salvataggio? L'operazione non si puo' annullare.",
             "Delete this save slot? This cannot be undone."},
            {StringKey::SAVE_DELETE_CONFIRM,   "Elimina",            "Delete"},
            {StringKey::SAVE_DELETE_FAILED,    "Eliminazione non riuscita",
                                               "Could not delete the save"},
            {StringKey::SAVE_RENAME_PROMPT,    "Rinomina la partita salvata:",
                                               "Rename game slot:"},
            {StringKey::SAVE_RENAME_CONFIRM,   "Rinomina",           "Rename"},
            {StringKey::SAVE_RENAME_FAILED,    "Rinomina non riuscita",
                                               "Could not rename the save"},
            {StringKey::SAVE_MANAGE_HINT,
             "INVIO per confermare  -  ESC per annullare",
             "ENTER to confirm  -  ESC to cancel"},
            {StringKey::MENU_HINT,             "INVIO per giocare  -  ESC per uscire",
                                               "ENTER to play  -  ESC to quit"},
            {StringKey::MENU_WINS,             "Vittorie",           "Wins"},
            {StringKey::MENU_LOSSES,           "Sconfitte",          "Losses"},
            {StringKey::MENU_LEVEL,            "Livello",            "Level"},

            // --- Rank titles ---
            {StringKey::RANK_ROOKIE,           "Novellino",          "Rookie"},
            {StringKey::RANK_HACKER,           "Hacker",             "Hacker"},
            {StringKey::RANK_MASTERMIND,       "Mastermind",         "Mastermind"},
            {StringKey::RANK_LEGEND,           "Leggenda",           "Legend"},
            {StringKey::MENU_LOAD_FAILED,      "Caricamento non riuscito",
                                               "Could not load the game"},
            {StringKey::MENU_SAVE_INVALID,     "Salvataggio non valido",
                                               "Invalid saved game"},

            // --- Local match ---
            {StringKey::GAME_PIE_RULE,         "Pie Rule",           "Pie Rule"},
            {StringKey::GAME_NEW_GAME,         "Nuova partita",      "New game"},
            {StringKey::GAME_SAVE,             "Salva",              "Save"},
            {StringKey::GAME_LEAVE,            "Abbandona",          "Leave game"},
            {StringKey::GAME_LEAVE_PROMPT,
             "Vuoi abbandonare la partita in corso?",
             "Leave the game in progress?"},
            {StringKey::GAME_LEAVE_HINT,
             "INVIO per confermare  -  ESC per annullare",
             "ENTER to confirm  -  ESC to cancel"},
            {StringKey::GAME_MENU_HINT_OVER,   "[Esc] Torna al menu","[Esc] Back to menu"},
            {StringKey::GAME_MENU_HINT,        "[Esc] Abbandona",    "[Esc] Leave game"},
            {StringKey::GAME_THINKING,         "sta pensando...",    "thinking..."},
            {StringKey::GAME_SAVED,            "Partita salvata",    "Game saved"},
            {StringKey::GAME_MOVES,            "mosse",              "moves"},
            {StringKey::GAME_SAVE_FAILED,      "Salvataggio non riuscito",
                                               "Could not save the game"},
            {StringKey::GAME_REPLAY_FAILED,
             "Salvataggio non riproducibile: si ricomincia da capo",
             "Saved game cannot be replayed: starting over"},
            {StringKey::GAME_SAVE_PROMPT,      "Nome del salvataggio:",
                                               "Save name:"},
            {StringKey::GAME_SAVE_NAME,        "La mia partita",     "My game"},
            {StringKey::GAME_SAVE_CONFIRM,     "Salva",              "Save"},
            {StringKey::GAME_CANCEL,           "Annulla",            "Cancel"},
            {StringKey::GAME_SAVE_BAD_NAME,
             "Usa lettere, cifre, spazi, - e _",
             "Use letters, digits, spaces, - and _"},
            {StringKey::GAME_SAVE_HINT,
             "INVIO per salvare  -  ESC per annullare",
             "ENTER to save  -  ESC to cancel"},
            {StringKey::ARCADE_BLITZ,
             "ARCADE  -  Blitz: 10 secondi a mossa, buchi neri sulla scacchiera",
             "ARCADE  -  Blitz: 10 seconds per move, black holes on the board"},

            // --- Match status ---
            {StringKey::STATUS_TO_MOVE,        "Muove:",             "To move:"},
            {StringKey::STATUS_TURN_OF,        "Turno:",             "Turn:"},
            {StringKey::STATUS_WON,            "Ha vinto:",          "Winner:"},
            {StringKey::NOTICE_PIE_SWAPPED,    "Pie Rule: colori scambiati",
                                               "Pie Rule: colours swapped"},
            {StringKey::NOTICE_UNDONE,         "Annullata",          "Undone"},
            {StringKey::NOTICE_UNDONE_END,     "Annullata la fine della partita",
                                               "Game ending undone"},
            {StringKey::NOTICE_ILLEGAL_MOVE,   "Mossa illegale di",  "Illegal move by"},
            {StringKey::NOTICE_TIMEOUT,        "Tempo scaduto per",  "Time out for"},
            {StringKey::NOTICE_PLAYER_ERROR,   "Errore di",          "Error from"},

            // --- End reasons ---
            {StringKey::REASON_CONNECTION,     "connessione",        "connection"},
            {StringKey::REASON_RESIGN,         "resa",               "resignation"},
            {StringKey::REASON_TIMEOUT,        "tempo scaduto",      "time out"},
            {StringKey::REASON_ILLEGAL,        "mossa illegale",     "illegal move"},
            {StringKey::REASON_UNKNOWN,        "sconosciuto",        "unknown"},

            // --- Settings ---
            {StringKey::SETTINGS_TITLE,        "Impostazioni",       "Settings"},
            {StringKey::SETTINGS_SUBTITLE,
             "Le scelte vengono ricordate al prossimo avvio",
             "Your choices are remembered next time"},
            {StringKey::SETTINGS_WINDOWED,     "Finestra",           "Windowed"},
            {StringKey::SETTINGS_FULLSCREEN,   "Schermo intero",     "Fullscreen"},
            {StringKey::SETTINGS_AUDIO_ON,     "Attivi",             "On"},
            {StringKey::SETTINGS_MUTED,        "Disattivati",        "Off"},
            {StringKey::SETTINGS_LABEL_DISPLAY, "Schermo",           "Display"},
            {StringKey::SETTINGS_LABEL_SFX,    "Effetti Sonori",     "Sound effects"},
            {StringKey::SETTINGS_LABEL_LANGUAGE, "Lingua",           "Language"},
            {StringKey::SETTINGS_LABEL_COLOURS, "Colori",            "Colours"},
            {StringKey::SETTINGS_LANGUAGE_IT,  "Italiano",           "Italiano"},
            {StringKey::SETTINGS_LANGUAGE_EN,  "English",            "English"},
            {StringKey::SETTINGS_COLOURS_STANDARD,
             "Colori normali",     "Standard colours"},
            {StringKey::SETTINGS_COLOURS_COLORBLIND,
             "Daltonismo",         "Colourblind"},

            // --- Board style ---
            {StringKey::SETTINGS_LABEL_PALETTE, "Stile Scacchiera",  "Board style"},
            {StringKey::PALETTE_CLASSIC,       "Classico",           "Classic"},
            {StringKey::PALETTE_TOXIC,         "Tossico",            "Toxic"},
            {StringKey::PALETTE_PRESTIGE,      "Prestigio",          "Prestige"},
            {StringKey::PALETTE_REQUIRES_LEVEL, "richiede Liv.",     "requires Lv."},
            {StringKey::SETTINGS_MUSIC,        "Volume Musica",      "Music volume"},
            {StringKey::SETTINGS_MUSIC_OFF,    "No",                 "Off"},
            {StringKey::SETTINGS_MUSIC_LOW,    "Bassa",              "Low"},
            {StringKey::SETTINGS_MUSIC_MEDIUM, "Media",              "Medium"},
            {StringKey::SETTINGS_MUSIC_HIGH,   "Alta",               "High"},

            // --- Tutorial 1: the goal ---
            {StringKey::MENU_TUTORIAL,         "Tutorial",           "Tutorial"},
            {StringKey::TUTORIAL_TITLE,        "Come si gioca",      "How to play"},
            {StringKey::TUTORIAL_STEP_GOAL,
             "Sei il Rosso: devi collegare il bordo in alto a quello in basso. "
             "Clicca la cella illuminata per cominciare.",
             "You are Red: connect the top edge to the bottom one. "
             "Click the glowing cell to begin."},
            {StringKey::TUTORIAL_STEP_CHAIN,
             "Ogni pedina si aggancia alle vicine. Continua la catena verso il basso.",
             "Each piece links to its neighbours. Keep the chain going downwards."},
            {StringKey::TUTORIAL_STEP_ALMOST,
             "Ancora una cella e i tuoi due lati saranno collegati.",
             "One more cell and your two sides will be connected."},
            {StringKey::TUTORIAL_DONE,
             "Fatto: i bordi sono collegati e il Rosso ha vinto. In Hex una partita "
             "non puo' finire in parita'.",
             "Done: the edges are connected and Red has won. In Hex a game can never "
             "end in a draw."},
            {StringKey::TUTORIAL_EXIT,         "Esc per tornare al menu",
                                               "Esc to go back to the menu"},

            // --- Tutorial 2: Blue ---
            {StringKey::TUTORIAL_TITLE_BLUE,   "Il turno del Blu",   "Playing as Blue"},
            {StringKey::TUTORIAL_BLUE_GOAL,
             "Ora sei il Blu: i tuoi bordi sono gli altri due. Devi collegare il lato "
             "sinistro a quello destro. Clicca la cella illuminata.",
             "Now you are Blue: your edges are the other two. Connect the left side to "
             "the right one. Click the glowing cell."},
            {StringKey::TUTORIAL_BLUE_CHAIN,
             "Stessa regola, direzione diversa: la catena del Blu cresce in orizzontale.",
             "Same rule, different direction: Blue's chain grows sideways."},
            {StringKey::TUTORIAL_BLUE_ALMOST,
             "Ancora una cella e il bordo destro sara' raggiunto.",
             "One more cell and the right edge is reached."},
            {StringKey::TUTORIAL_BLUE_DONE,
             "Fatto: il Blu ha collegato i suoi due lati. Ogni partita di Hex e' questa "
             "corsa fra due direzioni, e una delle due arriva sempre.",
             "Done: Blue has connected its two sides. Every game of Hex is this race "
             "between two directions, and one of them always gets there."},
            {StringKey::TUTORIAL_NEXT,         "Avanti: il turno del Blu",
                                               "Next: playing as Blue"},

            // --- Tutorial 3: the pie rule ---
            {StringKey::TUTORIAL_TITLE_PIE,    "La Pie Rule",        "The Pie Rule"},
            {StringKey::TUTORIAL_PIE_BODY,
             "Chi apre ha un vantaggio enorme: la prima pedina al centro e' cosi' forte "
             "che, giocata bene, la partita e' gia' decisa. La Pie Rule lo corregge. "
             "Al suo primo turno il Blu puo' rinunciare a muovere e prendersi la pedina "
             "appena giocata, scambiando i colori: e' per questo che la prima pedina puo' "
             "cambiare schieramento sotto i tuoi occhi. Non e' un errore del gioco, e' il "
             "Blu che ruba l'apertura. Conviene quindi aprire con una mossa mediocre: "
             "una troppo buona te la prendono.",
             "Whoever opens has a huge advantage: the first piece in the centre is so "
             "strong that, played well, the game is already decided. The Pie Rule fixes "
             "that. On their first turn Blue may decline to move and take the piece just "
             "played instead, swapping colours: that is why the first piece can change "
             "sides in front of you. It is not a bug, it is Blue stealing the opening. "
             "So it pays to open with a mediocre move: a good one gets taken."},
            {StringKey::TUTORIAL_NEXT_PIE,     "Avanti: la Pie Rule",
                                               "Next: the Pie Rule"},

            // --- Tutorial 4: Arcade mode ---
            {StringKey::TUTORIAL_TITLE_ARCADE, "Modalita' Arcade",   "Arcade mode"},
            {StringKey::TUTORIAL_ARCADE_BODY,
             "Arcade cambia due cose. Blitz: dieci secondi per ogni mossa, e chi li "
             "supera perde all'istante, umano o Computer che sia. Buchi neri: quattro "
             "celle vengono murate a caso prima della prima mossa, come quelle spente "
             "qui sopra. Non appartengono a nessuno dei due, non si possono giocare e "
             "non collegano niente: vanno aggirate.",
             "Arcade changes two things. Blitz: ten seconds per move, and whoever goes "
             "over loses on the spot, human or Computer alike. Black holes: four cells "
             "are walled off at random before the first move, like the dark ones above. "
             "They belong to neither side, cannot be played and connect nothing: you "
             "have to go around them."},
            {StringKey::TUTORIAL_NEXT_ARCADE,  "Avanti: la modalita' Arcade",
                                               "Next: Arcade mode"},
            {StringKey::SETTINGS_WRITE_FAILED,
             "Impossibile scrivere le preferenze: la scelta vale solo per questa sessione",
             "Could not write the preferences: this choice lasts for this session only"},

            // --- Network lobby ---
            {StringKey::LOBBY_TITLE,           "Gioca Online",       "Play Online"},
            {StringKey::LOBBY_HINT,
             "Serve un server in ascolto: avvia HEX_Server, oppure inserisci l'indirizzo di chi lo ospita",
             "A running server is required: start HEX_Server, or enter the address of the host"},
            {StringKey::LOBBY_ADDRESS,         "Indirizzo del server ",
                                               "Server address"},
            {StringKey::LOBBY_NAME,            "Il tuo nome",        "Your name"},
            {StringKey::LOBBY_CONNECT,         "Connetti",           "Connect"},
            {StringKey::LOBBY_CONNECTING,      "Connessione in corso...",
                                               "Connecting..."},
            {StringKey::LOBBY_WAITING,         "In attesa dell'avversario...",
                                               "Waiting for an opponent..."},
            {StringKey::LOBBY_CONNECT_FAILED,  "Connessione non riuscita",
                                               "Could not connect"},
            {StringKey::LOBBY_SERVER_CLOSED,   "Il server ha chiuso la connessione",
                                               "The server closed the connection"},
            {StringKey::LOBBY_CONNECTION_LOST, "Connessione persa",  "Connection lost"},
            {StringKey::LOBBY_BAD_START,       "Il server ha mandato un avvio incomprensibile",
                                               "The server sent an unreadable match start"},
            {StringKey::LOBBY_REFUSED,         "Il server ha rifiutato",
                                               "The server refused"},
            {StringKey::LOBBY_OPPONENT_LEFT,   "L'avversario ha lasciato la partita",
                                               "Your opponent left the game"},

            // --- Online match ---
            {StringKey::NET_RESIGN,            "Abbandona",          "Resign"},
            {StringKey::NET_LEAVE,             "Torna al menu",      "Back to menu"},
            {StringKey::NET_LEAVE_HINT,        "[Esc] Lascia la partita",
                                               "[Esc] Leave the game"},
            {StringKey::NET_OPPONENT,          "Avversario",         "Opponent"},
            {StringKey::NET_STILL_CONNECTED,
             "Partita conclusa - torna al menu quando vuoi",
             "Game over - go back to the menu when you like"},
            {StringKey::NET_YOUR_TURN,         "Tocca a te",         "Your turn"},
            {StringKey::NET_WAIT_FOR,          "Attendi",            "Waiting for"},
            {StringKey::NET_YOU_WON,           "Hai vinto",          "You won"},
            {StringKey::NET_HAS_WON,           "ha vinto",           "won"},
            {StringKey::NET_BAD_STATE,         "Il server ha mandato uno stato incoerente",
                                               "The server sent an inconsistent state"},
            {StringKey::NET_UNREADABLE_STATE,  "Il server ha mandato uno stato illeggibile",
                                               "The server sent an unreadable state"},
        }};

        /** @brief Returns the prebuilt phrase table for a language. */
        const std::array<std::string, STRING_COUNT>& tableFor(const Language language) {
            static const std::array<std::string, STRING_COUNT> italian = [] {
                std::array<std::string, STRING_COUNT> out;
                for (std::size_t i = 0; i < STRING_COUNT; ++i) out[i] = TABLE[i].it;
                return out;
            }();

            static const std::array<std::string, STRING_COUNT> english = [] {
                std::array<std::string, STRING_COUNT> out;
                for (std::size_t i = 0; i < STRING_COUNT; ++i) out[i] = TABLE[i].en;
                return out;
            }();

            return language == Language::EN ? english : italian;
        }
    }

    LocalizationManager::LocalizationManager(const Language language) : active(language) {}

    const std::string& LocalizationManager::text(const StringKey key) const {
        return textIn(active, key);
    }

    const std::string& LocalizationManager::textIn(const Language language, const StringKey key) {
        const auto index = static_cast<std::size_t>(key);

        // An out-of-range key is a programming error, not a missing phrase:
        // answer with an empty string rather than reading past the array.
        static const std::string empty;
        if (index >= STRING_COUNT) return empty;

        return tableFor(language)[index];
    }

    std::string_view LocalizationManager::codeOf(const Language language) {
        return language == Language::EN ? "en" : "it";
    }

    std::optional<Language> LocalizationManager::languageFromCode(const std::string_view code) {
        if (code == "it") return Language::IT;
        if (code == "en") return Language::EN;
        return std::nullopt;
    }
}

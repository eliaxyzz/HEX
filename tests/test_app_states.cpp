/**
 * @file test_app_states.cpp
 * @brief Application state machine and navigation rules.
 */

#include "states/app_state.h"

#include <memory>
#include <string>
#include <vector>

#include "test_framework.h"

using namespace hexapp;

namespace {

    /**
     * @brief Stub state recording its own lifecycle into a shared log, so the
     * tests can check the order of the calls.
     */
    class SpyState final : public AppState {
    public:
        SpyState(const StateId id, std::vector<std::string>& log)
            : state_id(id), log(&log) {}

        ~SpyState() override { log->push_back(name() + ":destroy"); }

        [[nodiscard]] StateId id() const override { return state_id; }

        void onEnter() override { log->push_back(name() + ":enter"); }
        void onExit() override { log->push_back(name() + ":exit"); }

        void handleInput(const InputEvent& event) override {
            log->push_back(name() + ":input");
            if (event.type != InputType::KEY_PRESSED) return;

            if (event.key == Key::ENTER) requestTransition(Transition::to(StateId::PLAYING));
            else if (event.key == Key::ESCAPE) requestTransition(Transition::to(StateId::MAIN_MENU));
            else if (event.key == Key::R) requestTransition(Transition::quit());
        }

        void update(const float dt) override {
            log->push_back(name() + ":update");
            elapsed += dt;
            if (transition_on_update) requestTransition(Transition::to(StateId::PLAYING));
        }

        /** @brief Sum of the dt values received. */
        float elapsed = 0.0f;

        /** @brief When true, requests a transition on the next update. */
        bool transition_on_update = false;

    private:
        [[nodiscard]] std::string name() const {
            return state_id == StateId::MAIN_MENU ? "menu" : "play";
        }

        StateId state_id;
        std::vector<std::string>* log;
    };

    /** @brief Counts how many times an entry appears in the log. */
    int countIn(const std::vector<std::string>& log, const std::string& entry) {
        int n = 0;
        for (const std::string& e : log) if (e == entry) ++n;
        return n;
    }
}

void app_states() {
    // --- Transitions: construction and comparison ---
    {
        const Transition go = Transition::to(StateId::PLAYING);
        CHECK(go.kind == Transition::Kind::GO_TO, "transizione: GO_TO");
        CHECK(go.target == StateId::PLAYING, "transizione: destinazione corretta");
        CHECK(Transition::quit().kind == Transition::Kind::QUIT, "transizione: QUIT");
        CHECK(go == Transition::to(StateId::PLAYING), "transizione: confrontabile");
        CHECK(!(go == Transition::to(StateId::MAIN_MENU)), "transizione: destinazioni diverse differiscono");
    }

    // --- Navigation rules, the same ones the screens use ---
    {
        CHECK(menuTransitionFor(InputEvent::keyPress(Key::ENTER)) == Transition::to(StateId::PLAYING),
              "menu: INVIO avvia la partita");
        CHECK(menuTransitionFor(InputEvent::keyPress(Key::ESCAPE)) == Transition::quit(),
              "menu: ESC esce dall'applicazione");
        CHECK(menuTransitionFor(InputEvent::keyPress(Key::S)) == std::nullopt,
              "menu: un tasto non mappato non fa nulla");
        CHECK(menuTransitionFor(InputEvent::mouseMove(10.0f, 10.0f)) == std::nullopt,
              "menu: il movimento del mouse non naviga");

        CHECK(playingTransitionFor(InputEvent::keyPress(Key::ESCAPE)) == Transition::to(StateId::MAIN_MENU),
              "partita: ESC torna al menu, non esce");
        CHECK(playingTransitionFor(InputEvent::keyPress(Key::ENTER)) == std::nullopt,
              "partita: INVIO non naviga");
        CHECK(playingTransitionFor(InputEvent::keyPress(Key::S)) == std::nullopt,
              "partita: i comandi di gioco non navigano");
        CHECK(playingTransitionFor(InputEvent::mousePress(4.0f, 4.0f)) == std::nullopt,
              "partita: il click non naviga");
    }

    // --- Starting the machine ---
    {
        std::vector<std::string> log;
        StateMachine<AppState> machine([&log](const StateId id) -> std::unique_ptr<AppState> {
            return std::make_unique<SpyState>(id, log);
        });

        CHECK(machine.current() == nullptr, "macchina: nessuno stato prima di start()");
        CHECK(machine.currentId() == std::nullopt, "macchina: nessun identificatore prima di start()");
        CHECK(machine.isRunning(), "macchina: in esecuzione alla creazione");

        machine.start(StateId::MAIN_MENU);
        CHECK(machine.currentId() == StateId::MAIN_MENU, "macchina: start() installa la schermata iniziale");
        CHECK(log.size() == 1 && log[0] == "menu:enter", "macchina: start() chiama onEnter");
        CHECK(machine.transitionCount() == 0, "macchina: nessuna transizione all'avvio");
    }

    // --- The transition does not happen inside handleInput ---
    {
        std::vector<std::string> log;
        StateMachine<AppState> machine([&log](const StateId id) -> std::unique_ptr<AppState> {
            return std::make_unique<SpyState>(id, log);
        });
        machine.start(StateId::MAIN_MENU);
        log.clear();

        machine.handleInput(InputEvent::keyPress(Key::ENTER));
        CHECK(machine.currentId() == StateId::MAIN_MENU,
              "macchina: lo stato non cambia dentro handleInput");
        CHECK(countIn(log, "menu:destroy") == 0, "macchina: lo stato non viene distrutto mentre e' in uso");

        const bool changed = machine.applyPendingTransition();
        CHECK(changed, "macchina: applyPendingTransition esegue il cambio");
        CHECK(machine.currentId() == StateId::PLAYING, "macchina: si passa alla partita");
        CHECK(machine.transitionCount() == 1, "macchina: una transizione contata");

        // Order: exit the old one, destroy it, enter the new one.
        const std::vector<std::string> expected{"menu:input", "menu:exit", "menu:destroy", "play:enter"};
        CHECK(log == expected, "macchina: onExit, distruzione e onEnter nell'ordine giusto");
    }

    // --- No pending transition: nothing to do ---
    {
        std::vector<std::string> log;
        StateMachine<AppState> machine([&log](const StateId id) -> std::unique_ptr<AppState> {
            return std::make_unique<SpyState>(id, log);
        });
        machine.start(StateId::MAIN_MENU);

        CHECK(!machine.applyPendingTransition(), "macchina: senza richieste non succede nulla");
        machine.handleInput(InputEvent::keyPress(Key::S));
        CHECK(!machine.applyPendingTransition(), "macchina: un tasto non mappato non genera transizioni");
        CHECK(machine.currentId() == StateId::MAIN_MENU, "macchina: si resta dove si era");
        CHECK(machine.transitionCount() == 0, "macchina: nessuna transizione contata");
    }

    // --- Repeated round trips ---
    {
        std::vector<std::string> log;
        StateMachine<AppState> machine([&log](const StateId id) -> std::unique_ptr<AppState> {
            return std::make_unique<SpyState>(id, log);
        });
        machine.start(StateId::MAIN_MENU);

        for (int i = 0; i < 20; ++i) {
            machine.handleInput(InputEvent::keyPress(Key::ENTER));
            machine.applyPendingTransition();
            CHECK_QUIET(machine.currentId() == StateId::PLAYING);

            machine.handleInput(InputEvent::keyPress(Key::ESCAPE));
            machine.applyPendingTransition();
            CHECK_QUIET(machine.currentId() == StateId::MAIN_MENU);
        }
        CHECK(quiet_failures == 0, "macchina: venti andate e ritorni fra menu e partita");
        quiet_failures = 0;

        CHECK(machine.transitionCount() == 40, "macchina: quaranta transizioni contate");
        // Every installed state is later destroyed; none is left dangling.
        CHECK(countIn(log, "menu:enter") + countIn(log, "play:enter")
              == countIn(log, "menu:destroy") + countIn(log, "play:destroy") + 1,
              "macchina: ogni schermata uscente viene distrutta, tranne quella attiva");
    }

    // --- Exiting the application ---
    {
        std::vector<std::string> log;
        StateMachine<AppState> machine([&log](const StateId id) -> std::unique_ptr<AppState> {
            return std::make_unique<SpyState>(id, log);
        });
        machine.start(StateId::MAIN_MENU);

        machine.handleInput(InputEvent::keyPress(Key::R));   // richiede QUIT
        CHECK(machine.isRunning(), "macchina: ancora in esecuzione prima di applicare");

        machine.applyPendingTransition();
        CHECK(!machine.isRunning(), "macchina: QUIT ferma l'applicazione");
        CHECK(machine.current() == nullptr, "macchina: dopo QUIT non c'e' piu' uno stato attivo");
        CHECK(countIn(log, "menu:exit") == 1, "macchina: onExit chiamato anche uscendo");
        CHECK(countIn(log, "menu:destroy") == 1, "macchina: lo stato viene distrutto uscendo");

        // Subsequent calls must break nothing.
        machine.handleInput(InputEvent::keyPress(Key::ENTER));
        machine.update(0.016f);
        CHECK(!machine.applyPendingTransition(), "macchina: dopo l'uscita e' tutto inerte");
    }

    // --- update() and delta time ---
    {
        std::vector<std::string> log;
        SpyState* spy = nullptr;
        StateMachine<AppState> machine([&](const StateId id) -> std::unique_ptr<AppState> {
            auto s = std::make_unique<SpyState>(id, log);
            spy = s.get();
            return s;
        });
        machine.start(StateId::MAIN_MENU);

        machine.update(0.016f);
        machine.update(0.024f);
        CHECK(spy->elapsed > 0.039f && spy->elapsed < 0.041f, "macchina: il delta time arriva allo stato");

        // A transition requested during update is honoured at the end of the frame too.
        spy->transition_on_update = true;
        machine.update(0.016f);
        CHECK(machine.currentId() == StateId::MAIN_MENU, "macchina: update non cambia stato da solo");
        machine.applyPendingTransition();
        CHECK(machine.currentId() == StateId::PLAYING, "macchina: la transizione chiesta in update viene eseguita");
    }

    // --- Two requests in one frame: the first wins ---
    {
        std::vector<std::string> log;
        StateMachine<AppState> machine([&log](const StateId id) -> std::unique_ptr<AppState> {
            return std::make_unique<SpyState>(id, log);
        });
        machine.start(StateId::MAIN_MENU);

        machine.handleInput(InputEvent::keyPress(Key::ENTER));   // -> PLAYING
        machine.handleInput(InputEvent::keyPress(Key::R));       // -> QUIT, ignorata
        machine.applyPendingTransition();

        CHECK(machine.isRunning(), "macchina: la seconda richiesta dello stesso frame viene ignorata");
        CHECK(machine.currentId() == StateId::PLAYING, "macchina: vale la prima richiesta");
        CHECK(machine.transitionCount() == 1, "macchina: una sola transizione eseguita");
    }
}

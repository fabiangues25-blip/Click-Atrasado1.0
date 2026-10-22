#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <deque>
using namespace geode::prelude;

// Unidad de "frame" en este mod = un paso de física (240 por segundo).
static bool g_simulando = false;
static bool g_murioSim = false;
constexpr int EXTRA_PASOS = 24; // pasos simulados después del clic

// Durante la simulación, la "muerte" solo se anota y no ocurre de verdad.
class $modify(MuerteSim, PlayLayer) {
    void destroyPlayer(PlayerObject* p, GameObject* o) {
        if (g_simulando) { g_murioSim = true; return; }
        PlayLayer::destroyPlayer(p, o);
    }
};

struct Evento { int frameAplicar; bool down; int button; bool p1; };

class $modify(AjusteLayer, GJBaseGameLayer) {
    struct Fields {
        std::deque<Evento> cola;
        int frame = 0;
        bool reinyectando = false;
    };

    // Guarda el estado, simula con el clic aplicado en 'offset' pasos,
    // restaura el estado y devuelve si sobrevivió.
    bool sobrevive(int offset, bool down, int button, bool p1) {
        auto pl = PlayLayer::get();
        if (!pl || !pl->m_player1 || pl->m_player1->m_isDead) return true;
        auto cp = pl->createCheckpoint();
        if (!cp) return true;
        g_simulando = true;
        g_murioSim = false;
        int total = offset + EXTRA_PASOS;
        for (int i = 0; i < total && !g_murioSim; ++i) {
            if (i == offset) this->handleButton(down, button, p1);
            this->processCommands(1.f / 240.f);
        }
        g_simulando = false;
        pl->loadFromCheckpoint(cp);
        return !g_murioSim;
    }

    void handleButton(bool down, int button, bool p1) {
        auto f = m_fields.self();
        if (g_simulando || f->reinyectando ||
            !Mod::get()->getSettingValue<bool>("enabled")) {
            return GJBaseGameLayer::handleButton(down, button, p1);
        }
        int maxD = Mod::get()->getSettingValue<int64_t>("delay-frames");
        int elegido = maxD;
        for (int d = 0; d <= maxD; ++d) {
            if (this->sobrevive(d, down, button, p1)) { elegido = d; break; }
        }
        f->cola.push_back({f->frame + elegido, down, button, p1});
    }

    void processCommands(float dt) {
        if (g_simulando) return GJBaseGameLayer::processCommands(dt);
        auto f = m_fields.self();
        f->frame++;
        f->reinyectando = true;
        while (!f->cola.empty() && f->cola.front().frameAplicar <= f->frame) {
            auto e = f->cola.front(); f->cola.pop_front();
            GJBaseGameLayer::handleButton(e.down, e.button, e.p1);
        }
        f->reinyectando = false;
        GJBaseGameLayer::processCommands(dt);
    }
};

#include <hyprland/src/render/decorations/IHyprWindowDecoration.hpp>

#include "WindowInverter.h"

class DecorationsWrapper : public IHyprWindowDecoration {
public:
    DecorationsWrapper(WindowInverter& inverter, std::unique_ptr<IHyprWindowDecoration>&& to_wrap)
        : IHyprWindowDecoration(nullptr),
          m_Inverter(inverter),
          m_Wrapped(std::move(to_wrap))
    { }

    std::unique_ptr<IHyprWindowDecoration> take() {
        return std::move(m_Wrapped);
    }

    virtual void draw(CMonitor* m, float a) {
        m_Inverter.SoftToggle(false);
        m_Wrapped->draw(m, a);
        m_Inverter.SoftToggle(true);
    }

    virtual SDecorationPositioningInfo getPositioningInfo() { return m_Wrapped->getPositioningInfo(); }
    virtual void                       onPositioningReply(const SDecorationPositioningReply& reply) { m_Wrapped->onPositioningReply(reply); }
    virtual eDecorationType            getDecorationType() { return m_Wrapped->getDecorationType(); }
    virtual void                       updateWindow(PHLWINDOW w) { m_Wrapped->updateWindow(w); }
    virtual void                       damageEntire() { m_Wrapped->damageEntire(); }
    virtual bool                       onInputOnDeco(const eInputType i, const Vector2D& v, std::any a = {}) { return m_Wrapped->onInputOnDeco(i, v, a); }
    virtual eDecorationLayer           getDecorationLayer() { return m_Wrapped->getDecorationLayer(); }
    virtual uint64_t                   getDecorationFlags() { return m_Wrapped->getDecorationFlags(); }
    virtual std::string                getDisplayName() { return m_Wrapped->getDisplayName(); }
private:
    std::unique_ptr<IHyprWindowDecoration> m_Wrapped;
    WindowInverter& m_Inverter;
};
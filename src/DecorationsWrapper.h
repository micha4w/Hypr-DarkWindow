#include <hyprland/src/render/decorations/IHyprWindowDecoration.hpp>

#include "WindowInverter.h"

class DecorationsWrapper : public IHyprWindowDecoration {
public:
    DecorationsWrapper(WindowInverter& inverter, UP<IHyprWindowDecoration>&& to_wrap, const PHLWINDOW& window)
        : IHyprWindowDecoration(window),
          m_Inverter(inverter),
          m_Wrapped(std::move(to_wrap))
    { }

    UP<IHyprWindowDecoration> take() {
        return std::move(m_Wrapped);
    }
    IHyprWindowDecoration* get() {
        return m_Wrapped.get();
    }

    virtual void draw(PHLMONITOR m, float const& a) {
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
    UP<IHyprWindowDecoration> m_Wrapped;
    WindowInverter& m_Inverter;
};

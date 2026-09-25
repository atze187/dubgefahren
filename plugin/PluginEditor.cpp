#include "plugin/PluginEditor.h"
#include "engine/Kit.h"
#include "plugin/KitFile.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"

namespace dg {

DubgefahrenEditor::DubgefahrenEditor(DubgefahrenProcessor& proc)
    : AudioProcessorEditor(proc), proc_(proc), pads_(proc), slotEditor_(proc), fx_(proc), perf_(proc)
{
    setLookAndFeel(&lnf_);
    addAndMakeVisible(content_);

    title_.setText("DUBGEFAHREN", juce::dontSendNotification);
    title_.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    title_.setColour(juce::Label::textColourId, ui::colours::accent);

    kitButton_.onClick = [this] { showKitMenu(); };
    importButton_.onClick = [this] { importKit(); };
    exportButton_.onClick = [this] { exportKit(); };
    panicButton_.onStateChange = [this] { setPanic(panicButton_.isDown()); };
    followFocus_.setToggleState(proc_.editorFollowsFocus(), juce::dontSendNotification);
    followFocus_.onClick = [this] { proc_.setEditorFollowsFocus(followFocus_.getToggleState()); };

    pads_.onSelect = [this](int s) { selectSlot(s); };
    pads_.onContextMenu = [this](int s) { showPadMenu(s); };
    slotEditor_.onRename = [this] { renameSlot(selectedSlot_); };

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &title_, &kitButton_, &importButton_, &exportButton_, &panicButton_, &followFocus_, &pads_, &slotEditor_, &fx_, &perf_ })
        content_.addAndMakeVisible(*c);

    content_.setSize(kBaseWidth, kBaseHeight);
    layoutContent();
    selectSlot(proc_.focusSlot());
    lastStateGeneration_ = proc_.stateGeneration();

    // uiScale() wird vorab gelesen: setResizeLimits() zwingt die noch 0x0 große
    // Editor-Bounds sofort auf die Mindestgröße, was über resized() einen Zwischenwert
    // in den Processor zurückschreibt. Der hier gemerkte Zielwert überlebt das.
    const float scale = proc_.uiScale();
    setResizable(true, true);
    setResizeLimits(kBaseWidth * 3 / 4, kBaseHeight * 3 / 4, kBaseWidth * 2, kBaseHeight * 2);
    getConstrainer()->setFixedAspectRatio(static_cast<double>(kBaseWidth) / kBaseHeight);
    setSize(juce::roundToInt(kBaseWidth * scale), juce::roundToInt(kBaseHeight * scale));
    startTimerHz(30);
}

DubgefahrenEditor::~DubgefahrenEditor()
{
    stopTimer();
    setPanic(false);
    setLookAndFeel(nullptr);
}

void DubgefahrenEditor::paint(juce::Graphics& g) { g.fillAll(ui::colours::background); }

void DubgefahrenEditor::resized()
{
    const float scale = static_cast<float>(getWidth()) / static_cast<float>(kBaseWidth);
    content_.setBounds(0, 0, kBaseWidth, kBaseHeight);
    content_.setTransform(juce::AffineTransform::scale(scale));
    proc_.setUiScale(scale);
}

void DubgefahrenEditor::layoutContent()
{
    auto r = juce::Rectangle<int>(0, 0, kBaseWidth, kBaseHeight).reduced(12);
    auto header = r.removeFromTop(36);
    title_.setBounds(header.removeFromLeft(220));
    panicButton_.setBounds(header.removeFromRight(90).reduced(2));
    header.removeFromRight(12);
    exportButton_.setBounds(header.removeFromRight(80).reduced(2));
    importButton_.setBounds(header.removeFromRight(80).reduced(2));
    kitButton_.setBounds(header.removeFromRight(110).reduced(2));
    followFocus_.setBounds(header.removeFromRight(170));

    perf_.setBounds(r.removeFromBottom(84));
    r.removeFromBottom(8);
    fx_.setBounds(r.removeFromBottom(96));
    r.removeFromBottom(8);
    pads_.setBounds(r.removeFromLeft(360));
    r.removeFromLeft(12);
    slotEditor_.setBounds(r);
}

void DubgefahrenEditor::selectSlot(int slot)
{
    if (slot < 0 || slot >= kNumSlots)
        return;
    selectedSlot_ = slot;
    slotEditor_.setSlot(slot);
    pads_.setSelected(slot);
}

void DubgefahrenEditor::refreshAll()
{
    pads_.refreshNames();
    slotEditor_.refreshName();
}

void DubgefahrenEditor::pollProcessorState()
{
    const int gen = proc_.stateGeneration();
    if (gen != lastStateGeneration_)
    {
        lastStateGeneration_ = gen;
        refreshAll();
        followFocus_.setToggleState(proc_.editorFollowsFocus(), juce::dontSendNotification);
    }
}

void DubgefahrenEditor::timerCallback()
{
    pollProcessorState();

    const int focus = proc_.focusSlot();
    pads_.setPadStates(proc_.activeMask(), proc_.latchedMask(), focus);

    // Während der Nutzer einen Knopf zieht, keinen Slot wechseln: SlotEditor::setSlot()
    // würde die gerade aktive SliderAttachment zerstören und die Geste würde auf dem
    // falschen Slot weiterlaufen (und beim Host offen bleiben). lastFocus_ bleibt
    // unverändert, damit der Wechsel im ersten Tick nach dem Loslassen nachgeholt wird.
    if (juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        return;

    if (focus != lastFocus_)
    {
        lastFocus_ = focus;
        if (proc_.editorFollowsFocus() && focus != selectedSlot_)
            selectSlot(focus);
    }
}

void DubgefahrenEditor::setPanic(bool down)
{
    if (auto* p = proc_.state().getParameter(pid::panic))
    {
        const float target = down ? 1.0f : 0.0f;
        if (p->getValue() == target)
            return;
        p->beginChangeGesture();
        p->setValueNotifyingHost(target);
        p->endChangeGesture();
    }
}

void DubgefahrenEditor::showMessage(const juce::String& title, const juce::String& text)
{
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, title, text);
}

void DubgefahrenEditor::maybeShowConfigWarning()
{
    const auto& info = proc_.kitFolder();
    if (configWarningShown_ || info.warning.isEmpty())
        return;
    configWarningShown_ = true;
    showMessage("Hinweis zur Config", info.warning + "\n\nVerwendet wird: " + info.folder.getFullPathName());
}

void DubgefahrenEditor::showKitMenu()
{
    maybeShowConfigWarning();
    juce::PopupMenu menu;
    menu.addItem(1, "Werks-Kit laden");
    menu.addSeparator();
    auto files = proc_.kitFolder().folder.findChildFiles(juce::File::findFiles, false, juce::String("*") + kKitExtension);
    files.sort();
    if (files.isEmpty())
        menu.addItem(2, "(keine Kits im Ordner)", false);
    for (int i = 0; i < files.size(); ++i)
        menu.addItem(100 + i, files[i].getFileNameWithoutExtension());

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(kitButton_),
                       [safe = juce::Component::SafePointer<DubgefahrenEditor>(this), files](int result) {
                           if (safe == nullptr || result == 0)
                               return;
                           if (result == 1)
                               safe->proc_.applyKit(makeFactoryKit());
                           else if (result >= 100)
                               safe->loadKit(files[result - 100]);
                           safe->refreshAll();
                       });
}

void DubgefahrenEditor::loadKit(const juce::File& file)
{
    const auto result = loadKitFile(file);
    if (!result.kit)
    {
        showMessage("Kit konnte nicht geladen werden", result.error);
        return;
    }
    proc_.applyKit(*result.kit);
    refreshAll();
}

void DubgefahrenEditor::importKit()
{
    maybeShowConfigWarning();
    chooser_ = std::make_unique<juce::FileChooser>("Kit importieren", proc_.kitFolder().folder,
                                                   juce::String("*") + kKitExtension);
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<DubgefahrenEditor>(this)](const juce::FileChooser& fc) {
                              if (safe == nullptr || fc.getResult() == juce::File())
                                  return;
                              safe->loadKit(fc.getResult());
                          });
}

void DubgefahrenEditor::exportKit()
{
    maybeShowConfigWarning();
    chooser_ = std::make_unique<juce::FileChooser>("Kit exportieren",
                                                   proc_.kitFolder().folder.getChildFile(juce::String("Mein Kit") + kKitExtension),
                                                   juce::String("*") + kKitExtension);
    chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe = juce::Component::SafePointer<DubgefahrenEditor>(this)](const juce::FileChooser& fc) {
                              if (safe == nullptr || fc.getResult() == juce::File())
                                  return;
                              const auto file = fc.getResult().withFileExtension(kKitExtension);
                              juce::String error;
                              if (!saveKitFile(safe->proc_.currentKit(), file, error))
                                  safe->showMessage("Kit konnte nicht gespeichert werden", error);
                          });
}

void DubgefahrenEditor::showPadMenu(int slot)
{
    juce::PopupMenu menu;
    menu.addItem(1, "Kopieren");
    menu.addItem(2, juce::String::fromUTF8("Einfügen"), clipboard_.has_value());
    menu.addItem(3, juce::String::fromUTF8("Auf Werkseinstellung zurücksetzen"));
    menu.addItem(4, "Umbenennen");
    menu.showMenuAsync(juce::PopupMenu::Options(),
                       [safe = juce::Component::SafePointer<DubgefahrenEditor>(this), slot](int result) {
                           if (safe == nullptr)
                               return;
                           auto& self = *safe;
                           switch (result)
                           {
                               case 1:
                                   self.clipboard_ = std::make_pair(self.proc_.currentKit().slots[static_cast<std::size_t>(slot)],
                                                                    self.proc_.slotName(slot));
                                   break;
                               case 2:
                                   if (self.clipboard_)
                                       self.proc_.setSlot(slot, self.clipboard_->first, self.clipboard_->second);
                                   break;
                               case 3:
                               {
                                   const Kit factory = makeFactoryKit();
                                   self.proc_.setSlot(slot, factory.slots[static_cast<std::size_t>(slot)],
                                                      juce::String::fromUTF8(factory.names[static_cast<std::size_t>(slot)].c_str()));
                                   break;
                               }
                               case 4:
                                   self.renameSlot(slot);
                                   break;
                               default:
                                   break;
                           }
                           self.refreshAll();
                       });
}

void DubgefahrenEditor::renameSlot(int slot)
{
    auto* window = new juce::AlertWindow("Slot umbenennen", "Neuer Name:", juce::MessageBoxIconType::NoIcon);
    window->addTextEditor("name", proc_.slotName(slot));
    window->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Abbrechen", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    window->enterModalState(true,
                            juce::ModalCallbackFunction::create(
                                [safe = juce::Component::SafePointer<DubgefahrenEditor>(this), window, slot](int result) {
                                    if (safe == nullptr || result != 1)
                                        return;
                                    const auto name = window->getTextEditorContents("name").trim();
                                    if (name.isNotEmpty())
                                        safe->proc_.setSlotName(slot, name);
                                    safe->refreshAll();
                                }),
                            true);
}

} // namespace dg

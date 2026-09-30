#include "plugin/PluginEditor.h"
#include "engine/Kit.h"
#include "plugin/KitFile.h"
#include "plugin/ParameterLayout.h"
#include "plugin/PluginProcessor.h"
#include "plugin/SampleFiles.h"
#include "plugin/ui/Fonts.h"

namespace dg {

namespace {
enum KitMenuId { kKitFactory = 1, kKitNone = 2, kKitEmpty = 3, kKitFileBase = 100 };
enum PadMenuId { kPadCopy = 1, kPadPaste, kPadReset, kPadRename, kPadClear };
enum SourceMenuId { kSourceSynth = 1, kSourceSampleDisabled, kSourceNoSamples, kSourceAddFile, kSourceSampleBase = 100 };
} // namespace

DubgefahrenEditor::DubgefahrenEditor(DubgefahrenProcessor& proc)
    : AudioProcessorEditor(proc), proc_(proc), pads_(proc), slotEditor_(proc), fx_(proc), perf_(proc)
{
    setLookAndFeel(&lnf_);
    addAndMakeVisible(content_);

    title_.setText("DUBGEFAHREN", juce::dontSendNotification);
    title_.setFont(ui::font(22.0f, true).withKerningFactor(0.12f));
    title_.setColour(juce::Label::textColourId, ui::colours::accent);

    cpuMeter_.setSource([this] { return proc_.cpuLoad(); });

    kitButton_.onClick = [this] { showKitMenu(); };
    importButton_.onClick = [this] { importKit(); };
    exportButton_.onClick = [this] { exportKit(); };
    panicButton_.onStateChange = [this] { setPanic(panicButton_.isDown()); };
    followFocus_.setToggleState(proc_.editorFollowsFocus(), juce::dontSendNotification);
    followFocus_.onClick = [this] { proc_.setEditorFollowsFocus(followFocus_.getToggleState()); };

    pads_.onSelect = [this](int s) { selectSlot(s); };
    pads_.onContextMenu = [this](int s) { showPadMenu(s); };
    pads_.onEmptyClick = [this](int s) { showSourceMenu(s); };
    slotEditor_.onRename = [this] { renameSlot(selectedSlot_); };
    slotEditor_.onChooseSample = [this] { showSampleMenu(selectedSlot_); };

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &title_, &cpuMeter_, &kitButton_, &importButton_, &exportButton_, &panicButton_, &followFocus_, &pads_, &slotEditor_, &fx_, &perf_ })
        content_.addAndMakeVisible(*c);

    content_.setSize(kBaseWidth, kBaseHeight);
    layoutContent();
    selectSlot(proc_.focusSlot());
    lastStateGeneration_ = proc_.stateGeneration();
    lastSoundMask_ = soundMask();

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

void DubgefahrenEditor::paint(juce::Graphics& g)
{
    ui::drawWindowBackground(g, getLocalBounds());
    // Schatten ragen über ihre Komponenten hinaus, deshalb zeichnet sie der Editor darunter,
    // im Koordinatensystem der skalierten content_-Komponente.
    g.addTransform(content_.getTransform());
    panelShadows_[0].render(g, slotEditor_.getBounds().toFloat());
    panelShadows_[1].render(g, fx_.getBounds().toFloat());
    panelShadows_[2].render(g, perf_.getBounds().toFloat());
}

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
    cpuMeter_.setBounds(header.removeFromLeft(90));
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
    slotEditor_.refresh();
}

void DubgefahrenEditor::pollProcessorState()
{
    // Der Quellentyp kann sich auch ohne setSlot ändern (generischer Host-Editor, Host-Undo),
    // deshalb zusätzlich zur stateGeneration vergleichen.
    const int gen = proc_.stateGeneration();
    const std::uint32_t sound = soundMask();
    if (gen != lastStateGeneration_ || sound != lastSoundMask_)
    {
        lastStateGeneration_ = gen;
        lastSoundMask_ = sound;
        refreshAll();
        followFocus_.setToggleState(proc_.editorFollowsFocus(), juce::dontSendNotification);
    }

    // Erst melden, wenn alle Ladeaufträge erledigt sind: so entsteht pro Ladevorgang nur ein Hinweis.
    if (proc_.hasPendingSampleLoads())
        return;
    const auto problems = proc_.takeSampleProblems();
    if (!problems.isEmpty())
        showMessage("Some samples could not be loaded", problems.joinIntoString("\n"));
}

std::uint32_t DubgefahrenEditor::soundMask() const
{
    std::uint32_t m = 0;
    for (int s = 0; s < kNumSlots; ++s)
        if (hasSound(proc_.slotSource(s)))
            m |= 1u << s;
    return m;
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

    // Bei offenem ComboBox-Popup (oder einem anderen modalen Component) ebenfalls keinen
    // Slot wechseln: sonst würde der Editor unter dem Popup weggezogen. PopupMenu zeigt sein
    // Fenster als aktuell modale Component an; ein eigenes getNumCurrentlyModalMenus() gibt
    // es in JUCE 8.0.15 nicht, daher genügt die Prüfung auf getCurrentlyModalComponent().
    if (juce::Component::getCurrentlyModalComponent() != nullptr)
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
    lastMessage_ = title + "\n" + text;
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

juce::PopupMenu DubgefahrenEditor::buildKitMenu(const juce::Array<juce::File>& kitFiles) const
{
    juce::PopupMenu menu;
    menu.addItem(kKitFactory, "Load Factory Kit");
    menu.addItem(kKitEmpty, "New Empty Kit");
    menu.addSeparator();
    if (kitFiles.isEmpty())
        menu.addItem(kKitNone, "(no kits in folder)", false);
    for (int i = 0; i < kitFiles.size(); ++i)
        menu.addItem(kKitFileBase + i, kitFiles[i].getFileNameWithoutExtension());
    return menu;
}

void DubgefahrenEditor::showKitMenu()
{
    maybeShowConfigWarning();
    auto files = proc_.kitFolder().folder.findChildFiles(juce::File::findFiles, false, juce::String("*") + kKitExtension);
    files.sort();
    buildKitMenu(files).showMenuAsync(juce::PopupMenu::Options().withTargetComponent(kitButton_),
                                      [safe = juce::Component::SafePointer<DubgefahrenEditor>(this), files](int result) {
                                          if (safe == nullptr || result == 0)
                                              return;
                                          if (result == kKitFactory)
                                              safe->proc_.applyKit(makeFactoryKit(), {});
                                          else if (result == kKitEmpty)
                                              safe->proc_.applyKit(makeEmptyKit(), {});
                                          else if (result >= kKitFileBase)
                                              safe->loadKit(files[result - kKitFileBase]);
                                          safe->refreshAll();
                                      });
}

void DubgefahrenEditor::loadKit(const juce::File& file)
{
    const auto result = loadKitFile(file);
    if (!result.kit)
    {
        showMessage("Could not load kit", result.error);
        return;
    }
    proc_.applyKit(*result.kit, file);
    refreshAll();
}

void DubgefahrenEditor::importKit()
{
    maybeShowConfigWarning();
    chooser_ = std::make_unique<juce::FileChooser>("Import Kit", proc_.kitFolder().folder,
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
    chooser_ = std::make_unique<juce::FileChooser>("Export Kit",
                                                   proc_.kitFolder().folder.getChildFile(juce::String("My Kit") + kKitExtension),
                                                   juce::String("*") + kKitExtension);
    chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe = juce::Component::SafePointer<DubgefahrenEditor>(this)](const juce::FileChooser& fc) {
                              if (safe == nullptr || fc.getResult() == juce::File())
                                  return;
                              const auto file = fc.getResult().withFileExtension(kKitExtension);
                              safe->exportKitTo(file);
                          });
}

bool DubgefahrenEditor::exportKitTo(const juce::File& file)
{
    const Kit kit = proc_.currentKit();
    juce::StringArray problems;
    if (file != proc_.kitFile())
        problems = copyKitSamples(kit, proc_.sampleFolder(), sampleFolderFor(file));
    juce::String error;
    if (!saveKitFile(kit, file, error))
    {
        showMessage("Could not save kit", error);
        return false;
    }
    proc_.setKitFile(file);
    if (!problems.isEmpty())
        showMessage("Some samples could not be copied", problems.joinIntoString("\n"));
    return true;
}

juce::PopupMenu DubgefahrenEditor::buildPadMenu(int slot) const
{
    const bool empty = !hasSound(proc_.slotSource(slot));
    juce::PopupMenu menu;
    menu.addItem(kPadCopy, "Copy");
    menu.addItem(kPadPaste, "Paste", clipboard_.has_value());
    menu.addItem(kPadReset, "Reset to Factory Default");
    menu.addItem(kPadRename, "Rename", !empty);
    menu.addItem(kPadClear, "Clear Slot", !empty);
    return menu;
}

void DubgefahrenEditor::showPadMenu(int slot)
{
    buildPadMenu(slot).showMenuAsync(juce::PopupMenu::Options(),
                                     [safe = juce::Component::SafePointer<DubgefahrenEditor>(this), slot](int result) {
                                         if (safe == nullptr)
                                             return;
                                         auto& self = *safe;
                                         switch (result)
                                         {
                                             case kPadCopy:
                                                 self.clipboard_ = ClipboardSlot { self.proc_.currentKit().slots[static_cast<std::size_t>(slot)],
                                                                                   self.proc_.slotName(slot), self.proc_.slotSample(slot) };
                                                 break;
                                             case kPadPaste:
                                                 if (self.clipboard_)
                                                     self.proc_.setSlot(slot, self.clipboard_->params, self.clipboard_->name, self.clipboard_->sample);
                                                 break;
                                             case kPadReset:  self.proc_.resetSlotToFactory(slot); break;
                                             case kPadRename: self.renameSlot(slot); break;
                                             case kPadClear:  self.proc_.clearSlot(slot); break;
                                             default: break;
                                         }
                                         self.refreshAll();
                                     });
}

juce::PopupMenu DubgefahrenEditor::buildSourceMenu(const juce::StringArray& sampleFiles) const
{
    juce::PopupMenu menu;
    menu.addItem(kSourceSynth, "Synth");
    if (proc_.kitFile() == juce::File())
        menu.addItem(kSourceSampleDisabled, "Sample (export the kit first)", false);
    else
        menu.addSubMenu("Sample", buildSampleMenu(sampleFiles));
    return menu;
}

juce::PopupMenu DubgefahrenEditor::buildSampleMenu(const juce::StringArray& sampleFiles)
{
    juce::PopupMenu menu;
    if (sampleFiles.isEmpty())
        menu.addItem(kSourceNoSamples, "(no samples in kit folder)", false);
    for (int i = 0; i < sampleFiles.size(); ++i)
        menu.addItem(kSourceSampleBase + i, sampleFiles[i]);
    menu.addSeparator();
    menu.addItem(kSourceAddFile, juce::String::fromUTF8("Add File…"));
    return menu;
}

void DubgefahrenEditor::showSourceMenu(int slot)
{
    // Gleiche Optionen wie das Kit-Menü, verankert am angeklickten Pad.
    const auto files = listSampleFiles(proc_.sampleFolder());
    buildSourceMenu(files).showMenuAsync(juce::PopupMenu::Options().withTargetComponent(pads_.pad(slot)),
                                         [safe = juce::Component::SafePointer<DubgefahrenEditor>(this), slot, files](int result) {
                                             if (safe != nullptr)
                                                 safe->chooseSource(slot, result, files);
                                         });
}

void DubgefahrenEditor::showSampleMenu(int slot)
{
    const auto files = listSampleFiles(proc_.sampleFolder());
    buildSampleMenu(files).showMenuAsync(juce::PopupMenu::Options().withTargetComponent(slotEditor_.sampleButton()),
                                         [safe = juce::Component::SafePointer<DubgefahrenEditor>(this), slot, files](int result) {
                                             if (safe != nullptr)
                                                 safe->chooseSource(slot, result, files);
                                         });
}

void DubgefahrenEditor::chooseSource(int slot, int result, const juce::StringArray& files)
{
    if (result == kSourceSynth)
        proc_.resetSlotToFactory(slot);
    else if (result == kSourceAddFile)
    {
        addSampleFile(slot);
        return;
    }
    else if (result >= kSourceSampleBase && result - kSourceSampleBase < files.size())
        proc_.setSlotSample(slot, files[result - kSourceSampleBase]);
    else
        return;
    refreshAll();
}

void DubgefahrenEditor::addSampleFile(int slot)
{
    chooser_ = std::make_unique<juce::FileChooser>("Add Sample", juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                                                   sampleFileWildcard());
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<DubgefahrenEditor>(this), slot](const juce::FileChooser& fc) {
                              if (safe == nullptr || fc.getResult() == juce::File())
                                  return;
                              juce::String error;
                              const auto target = importSampleFile(fc.getResult(), safe->proc_.sampleFolder(), error);
                              if (target == juce::File())
                              {
                                  safe->showMessage("Could not add sample", error);
                                  return;
                              }
                              safe->proc_.setSlotSample(slot, target.getFileName());
                              safe->refreshAll();
                          });
}

void DubgefahrenEditor::renameSlot(int slot)
{
    auto* window = new juce::AlertWindow("Rename Slot", "New name:", juce::MessageBoxIconType::NoIcon);
    window->addTextEditor("name", proc_.slotName(slot));
    window->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
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

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

namespace fk::plugins::ui
{

/// Auswahlliste für einen Choice-Parameter (Wellenform, Kennlinie, Mikrofontyp), Name oben.
/// Bindung über ComboBoxParameterAttachment; die Einträge kommen aus dem Parameter selbst.
class Choice final : public juce::Component
{
public:
    explicit Choice (juce::AudioParameterChoice& parameter);

    void resized() override;

    [[nodiscard]] juce::ComboBox& getComboBox() noexcept;

private:
    juce::Label nameLabel;
    juce::ComboBox comboBox;
    // Erst nach addItemList anlegen: Die Bindung setzt beim Start den gewählten Eintrag.
    std::unique_ptr<juce::ComboBoxParameterAttachment> attachment;
};

} // namespace fk::plugins::ui

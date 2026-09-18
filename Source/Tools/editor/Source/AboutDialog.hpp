#pragma once

#include <QDialog>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct SDL_AudioStream;

namespace Ui
{
    class AboutDialog;
}

class AboutDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit AboutDialog(QWidget *parent = nullptr);
    ~AboutDialog();
private:
    void stopMusic();
    void startMusic();
private slots:
    void DoText();
private:
    Ui::AboutDialog *ui;
    std::string mScrollText;
    static void FeedAudio(void* userdata, SDL_AudioStream* stream, int additionalAmount, int totalAmount);

    std::vector<int16_t> mPcm;
    size_t mPcmPos = 0;
    SDL_AudioStream* mAudioStream = nullptr;
    bool mSdlAudioInited = false;
};

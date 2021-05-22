#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>

#include <Windows.h>
#include <xaudio2.h>

#include "wav.h"

void print_help() {
	std::cout << "Usage:\nkeypogger [sound file]\n";
}

// Taken from https://stackoverflow.com/questions/2573834/c-convert-string-or-char-to-wstring-or-wchar-t
std::wstring string_to_wstring(const std::string& str)
{
	if (str.empty())
		return std::wstring();

	size_t charsNeeded = MultiByteToWideChar(CP_UTF8, 0,
		str.data(), (int)str.size(), NULL, 0);
	if (charsNeeded == 0) {
		throw std::runtime_error("Failed converting UTF-8 string to UTF-16");
	}

	std::vector<wchar_t> buffer(charsNeeded);
	int charsConverted = ::MultiByteToWideChar(CP_UTF8, 0,
		str.data(), (int)str.size(), &buffer[0], buffer.size());
	if (charsConverted == 0) {
		throw std::runtime_error("Failed converting UTF-8 string to UTF-16");
	}

	return std::wstring(&buffer[0], charsConverted);
}

const int MAX_SOUNDS = 25;

struct AudioPlayer {
	IXAudio2* x_audio = nullptr;
	IXAudio2MasteringVoice* mastering_voice = nullptr;
	WAVEFORMATEXTENSIBLE wav_format = { 0 };
	XAUDIO2_BUFFER buffer = { 0 };
	std::vector<IXAudio2SourceVoice*> voices;
	int active_voice = 0;
};

AudioPlayer audio_init(const std::wstring& sound_file) {
	HRESULT hr;
	AudioPlayer player;

	hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr)) {
		throw std::runtime_error("Failed to initialize COM");
	}

	hr = XAudio2Create(&player.x_audio, 0, XAUDIO2_DEFAULT_PROCESSOR);
	if (FAILED(hr)) {
		throw std::runtime_error("Failed to initialize XAudio2");
	}

	IXAudio2MasteringVoice* mastering_voice = nullptr;
	hr = player.x_audio->CreateMasteringVoice(&player.mastering_voice);
	if (FAILED(hr)) {
		throw std::runtime_error("Failed to create mastering voice");
	}

	HANDLE h_file = CreateFile(
		sound_file.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		0,
		NULL
	);

	if (h_file == INVALID_HANDLE_VALUE) {
		std::cout << GetLastError();
		throw std::runtime_error("Failed to open file");
	}

	DWORD fp_result = SetFilePointer(h_file, 0, NULL, FILE_BEGIN);
	if (fp_result == INVALID_SET_FILE_POINTER) {
		throw std::runtime_error("Failed to seek file");
	}

	DWORD chunk_size;
	DWORD chunk_pos;
	FindChunk(h_file, fourccRIFF, chunk_size, chunk_pos);
	
	DWORD filetype;
	ReadChunkData(h_file, &filetype, sizeof(DWORD), chunk_pos);
	if (filetype != fourccWAVE) {
		throw std::runtime_error("The given file is not a valid .wav file");
	}

	FindChunk(h_file, fourccFMT, chunk_size, chunk_pos);
	ReadChunkData(h_file, &player.wav_format, chunk_size, chunk_pos);

	FindChunk(h_file, fourccDATA, chunk_size, chunk_pos);
	BYTE* wav_buffer = new BYTE[chunk_size];
	ReadChunkData(h_file, wav_buffer, chunk_size, chunk_pos);

	player.buffer.AudioBytes = chunk_size;
	player.buffer.pAudioData = wav_buffer;
	player.buffer.Flags = XAUDIO2_END_OF_STREAM;

	player.voices.resize(MAX_SOUNDS);

	for (int i = 0; i < MAX_SOUNDS; i++) {
		hr = player.x_audio->CreateSourceVoice(
			&player.voices[i],
			(WAVEFORMATEX*)&player.wav_format
		);
		if (FAILED(hr)) {
			throw std::runtime_error("Failed to create source voice");
		}
	}

	return player;
}

AudioPlayer player;

LRESULT CALLBACK on_key(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode < 0) {
		return CallNextHookEx(NULL, nCode, wParam, lParam);
	}

	if (wParam == WM_KEYDOWN) {
		player.voices[player.active_voice]->Stop();
		player.voices[player.active_voice]->SubmitSourceBuffer(&player.buffer);
		player.voices[player.active_voice]->Start();
		player.active_voice = (player.active_voice + 1) % MAX_SOUNDS;
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);

}

int main(int argc, char** argv) {
	if (argc != 2) {
		print_help();
		return 1;
	}

	std::string sound_file = std::string(argv[1]);
	std::wstring sound_file_w = string_to_wstring(sound_file);

	std::cout << "======== keypogger =======\n";
	std::cout << "Using sound file " << sound_file << "...\n";
	
	try {
		player = audio_init(sound_file_w);
	}
	catch (std::runtime_error& e) {
		std::cout << "EXCEPTION: " << e.what() << "\n";
		return 1;
	}

	std::cout << "Audio initialized...\n";

	HHOOK hook = SetWindowsHookEx(WH_KEYBOARD_LL, on_key, NULL, 0);

	MSG message;
	while (GetMessage(&message, NULL, 0, 0) != 0)
	{
		TranslateMessage(&message);
		DispatchMessage(&message);
	}

	UnhookWindowsHookEx(hook);
	std::cout << "Exiting...\n";

	return 0;
}
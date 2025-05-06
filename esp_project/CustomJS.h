const char custom[] PROGMEM = R"raw(
class LoaderWidget extends WidgetBase {
	constructor(data) {
		super(data, !!data.label);

		super.addOutput(Component.make('div', {
			class: 'loader'
		}));
	}

	static css = `
        .loader {
          aspect-ratio: 1;
          border-radius: 50%;
          background: 
            radial-gradient(farthest-side, var(--font_tint) 94%, #0000) top / 4px 4px no-repeat,
            conic-gradient(#0000 30%, var(--font_tint));
          -webkit-mask: radial-gradient(farthest-side, #0000 calc(100% - 4px), #000 0);
          animation: l13 1s infinite linear;
          width: 1em;
          height: 1em;
        }
        
        @keyframes l13 { 
			100% { transform: rotate(1turn); } 
		}`;
}

class VoiceWidget extends WidgetBase {
	constructor(data) {
		super(data, !!data.label);

		const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
		if (!SpeechRecognition) {
			popup("Web Speech API не поддерживается в этом браузере.");
			return;
		}

		popup("Web Speech API поддерживается в этом браузере.", false);

		this.recognition = new SpeechRecognition();
		this.recognition.continuous = true;
		this.recognition.interimResults = false;
		this.recognition.lang = 'ru-RU';

		this.isListening = false;
		this.fullTranscript = '';

		function makeSVG(tag, attrs = {}) {
			const el = document.createElementNS("http://www.w3.org/2000/svg", tag);
			for (let key in attrs) {
				el.setAttribute(key, attrs[key]);
			}
			return el;
		}

		this.$voiceIcon = makeSVG('svg', {
			class: 'voice-icon',
			width: '1em',
			height: '1em',
			fill: 'white',
			viewBox: '0 0 16 16'
		});

		this.$frstPath = makeSVG('path', {
			d: 'M5 3a3 3 0 0 1 6 0v5a3 3 0 0 1-6 0z'
		});

		this.$scndPath = makeSVG('path', {
			d: 'M3.5 6.5A.5.5 0 0 1 4 7v1a4 4 0 0 0 8 0V7a.5.5 0 0 1 1 0v1a5 5 0 0 1-4.5 4.975V15h3a.5.5 0 0 1 0 1h-7a.5.5 0 0 1 0-1h3v-2.025A5 5 0 0 1 3 8V7a.5.5 0 0 1 .5-.5'
		});

		this.$voiceIcon.appendChild(this.$frstPath);
		this.$voiceIcon.appendChild(this.$scndPath);

		this.$button = Component.make('div', {
			class: 'voice-button',
			context: this,
			events: {
				click: () => {
					if (this.isListening) {
						this.recognition.stop();
						this.$voiceIcon.classList.remove('active');
					} else {
						this.fullTranscript = '';
						this.recognition.start();
						this.$voiceIcon.classList.add('active');
					}
					this.isListening = !this.isListening;
				}
			}
		});

		this.$button.appendChild(this.$voiceIcon);

		super.addOutput(this.$button);

		this.recognition.onresult = (event) => {
			const result = event.results[0][0].transcript.trim();
			this.fullTranscript = result;
		};

		this.recognition.onend = () => {
			if (!this.isListening && this.fullTranscript) {
				this.sendValue(this.fullTranscript);
			}
		};

		this.update(data.value);
	}

	update(value) {
		console.log('Распознанный текст:', value);
	}

	updateColor(value) {
		if (!this.isListening) {
			this.$button.style.backgroundColor = intToColor(value);
		}
	}

	static css = `
        .voice-button {
            display: inline-flex;
            align-items: center;
            justify-content: center;
            background-color: var(--accent, #007bff);
            border: none;
            border-radius: 50%;
            padding: 0.5em;
            cursor: pointer;
            transition: background-color 0.3s ease;
        }

        .voice-icon.active {
            color: red;
            animation: pulse 1.2s infinite;
        }

        @keyframes pulse {
            0% { transform: scale(0.9); opacity: 0.7; }
            50% { transform: scale(1.3); opacity: 1; }
            100% { transform: scale(0.9); opacity: 0.7; }
        }`;
}
)raw";

class CustomBuilder: public sets::Builder {
public:
    CustomBuilder(const sets::Builder& original) : sets::Builder(original) {}
    
    bool Loader(size_t id, Text label = "") {
        BSON p;
        p["label"] = label;
        return this->Custom("LoaderWidget", id, p);
    }

    bool Loader(Text label = "") {
        BSON p;
        p["label"] = label;
        return this->Custom("LoaderWidget", _NO_ID, p);
    }

	bool Voice(size_t id, Text label = "") {
		BSON p;
		p["label"] = label;
		return this->Custom("VoiceWidget", id, p);
	}
};
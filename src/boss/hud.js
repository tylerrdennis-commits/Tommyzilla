import './banner.css';

export function mountBossHud(playHint, onNext) {
  const banner = document.createElement('div');
  banner.id = 'boss-banner';
  const button = document.createElement('button');
  button.id = 'next-boss';
  button.type = 'button';
  button.textContent = 'Next boss';
  playHint.prepend(button);
  playHint.prepend(banner);
  button.addEventListener('click', () => onNext());
  return {
    show(text) {
      banner.textContent = text;
    },
    text() {
      return banner.textContent;
    },
  };
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
////////////////////////////////////////////////////////////////////////////////////////////////////

// SPDX-FileCopyrightText: German Aerospace Center (DLR) <cosmoscout@dlr.de>
// SPDX-License-Identifier: MIT

(() => {
  class MesseApi {
    constructor() {
      this._root = null;
      this._img = null;
      this._phase = null;
      this._timeLabel = null;
      this._time = null;
      this._hint = null;
      this._currentGif = '';
    }

    init() {
      if (this._root) {
        return;
      }

      this._root = CosmoScout.gui.loadTemplateContent('csp-messe-template');
      document.getElementById('cosmoscout').appendChild(this._root);

      this._img = this._root.querySelector('.csp-messe-gif');
      this._phase = this._root.querySelector('.csp-messe-phase');
      this._timeLabel = this._root.querySelector('.csp-messe-time-label');
      this._time = this._root.querySelector('.csp-messe-time');
      this._hint = this._root.querySelector('.csp-messe-hint');
    }

    deinit() {
      if (!this._root) {
        return;
      }

      this._root.remove();
      this._root = null;
      this._img = null;
      this._phase = null;
      this._timeLabel = null;
      this._time = null;
      this._hint = null;
      this._currentGif = '';
    }

    setState(payloadJSON) {
      if (!this._root) {
        this.init();
      }

      const data = typeof payloadJSON === 'string' ? JSON.parse(payloadJSON) : payloadJSON;

      this._root.classList.toggle('is-hidden', !data.visible);
      this._root.dataset.phase = data.phaseClass ?? 'idle';

      this._phase.textContent = data.phase ?? 'Bereit';
      this._timeLabel.textContent = data.timeLabel ?? '';
      this._time.textContent = data.remaining ?? '--:--';
      this._hint.textContent = data.hint ?? '';

      if (data.hudWidth) {
        this._img.style.width = `${data.hudWidth}px`;
      }

      if (data.hudHeight) {
        this._img.style.height = `${data.hudHeight}px`;
      }

      if (data.gifPath && data.gifPath !== this._currentGif) {
        this._img.src = data.gifPath;
        this._currentGif = data.gifPath;
      }
    }
  }

  CosmoScout.messe = new MesseApi();
  CosmoScout.messe.init();
})();

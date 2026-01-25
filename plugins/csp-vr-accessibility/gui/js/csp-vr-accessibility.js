////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
////////////////////////////////////////////////////////////////////////////////////////////////////

// SPDX-FileCopyrightText: German Aerospace Center (DLR) <cosmoscout@dlr.de>
// SPDX-License-Identifier: MIT

(() => {

    /**
     * ////////////////////////////////////////////////////////////////////////////////////////////////////
     * View Offset Api (Icaros Pitch)
     */
    class ViewOffsetApi extends IApi {
        /**
         * @inheritDoc
         */
        name = 'viewOffset';

        /**
         * @inheritDoc
         */
        init() {
            // Initialisiert den Slider.
            // Parameter: Name des Callbacks, Min, Max, Step, [Standardwert]
            // Wir erlauben -90 bis +90 Grad.
            CosmoScout.gui.initSlider("viewOffset.setPitch", -90, 90, 1, [0]);
        }
    }

    /**
 * ////////////////////////////////////////////////////////////////////////////////////////////////////
 * Motion Points Api
 */
    class MotionPointsApi extends IApi {
        /**
         * @inheritDoc
         */
        name = 'motionPoints';
        picker;

        /**
         * @inheritDoc
         */
        init() {
            CosmoScout.gui.initSlider("motionPoints.setCount", 0, 3000, 50, [500]);
            CosmoScout.gui.initSlider("motionPoints.setRadius", 0.5, 50.0, 0.5, [5.0]);
            CosmoScout.gui.initSlider("motionPoints.setSize", 0.5, 20.0, 0.5, [4.0]);

            this.picker = document.querySelector('#motionPoints-setColor');

            this.picker.picker = new CP(this.picker);
            this.picker.picker.self.classList.add('no-alpha');

            this.picker.picker.on('drag', (r, g, b, a) => {
                const color = CP.HEX([r, g, b, 1]);
                this.picker.style.background = color;
                this.picker.value = color;

                CosmoScout.callbacks.motionPoints.setColor(color);
            });

            this.picker.oninput = (e) => {
                const color = CP.HEX(e.target.value);
                this.picker.picker.set(color[0], color[1], color[2], 1);
                this.picker.style.background = CP.HEX([color[0], color[1], color[2], 1]);

                CosmoScout.callbacks.motionPoints.setColor(e.target.value);
            };
        }

        setEnabledValue(value) {
            document.querySelector('#motionPoints-enabled').checked = value;
        }

        setCountValue(value) {
            document.querySelector('#motionPoints-setCount').value = value;
        }

        setRadiusValue(value) {
            document.querySelector('#motionPoints-setRadius').value = value;
        }

        setSizeValue(value) {
            document.querySelector('#motionPoints-setSize').value = value;
        }

        setColorValue(color) {
            this.picker.picker.set(CP.HEX(color));
            this.picker.style.background = color;
            this.picker.value = color;
        }
    }


    /**
    * ////////////////////////////////////////////////////////////////////////////////////////////////////
    * VirtualHorizon Api
    */
    class VirtualHorizonApi extends IApi {
        /**
         * @inheritDoc
         */
        name = 'virtualHorizon';
        picker;

        /**
         * @inheritDoc
         */
        init() {
            CosmoScout.gui.initSlider("virtualHorizon.setSize", 0.1, 2, 0.1, [0.5]);
            CosmoScout.gui.initSlider("virtualHorizon.setExtent", 1, 20, 0.1, [10]);
            CosmoScout.gui.initSlider("virtualHorizon.setAlpha", 0, 1, 0.01, [1]);
            this.picker = document.querySelector('#virtualHorizon-setColor');

            this.picker.picker = new CP(this.picker);
            this.picker.picker.self.classList.add('no-alpha');
            this.picker.picker.on('drag', (r, g, b, a) => {
                const color = CP.HEX([r, g, b, 1]);
                this.picker.style.background = color;
                this.picker.value = color;

                CosmoScout.callbacks.virtualHorizon.setColor(color);
            });
            this.picker.oninput = (e) => {
                const color = CP.HEX(e.target.value);
                this.picker.picker.set(color[0], color[1], color[2], 1);
                this.picker.style.background = CP.HEX([color[0], color[1], color[2], 1]);

                CosmoScout.callbacks.virtualHorizon.setColor(e.target.value);
            };
        }

        setColorValue(color) {
            this.picker.picker.set(CP.HEX(color));
            this.picker.style.background = color;
            this.picker.value = color;
        }
    }

    /**
 * ////////////////////////////////////////////////////////////////////////////////////////////////////
 * Crosshair Api
 */
    class CrosshairApi extends IApi {
        /**
         * @inheritDoc
         */
        name = 'crosshair';
        picker;

        /**
         * @inheritDoc
         */
        init() {
            CosmoScout.gui.initSlider("crosshair.setSize", 0.1, 2, 0.1, [0.5]);
            CosmoScout.gui.initSlider("crosshair.setExtent", 1, 20, 0.1, [10]);
            CosmoScout.gui.initSlider("crosshair.setAlpha", 0, 1, 0.01, [1]);
            CosmoScout.gui.initSlider("crosshair.setRollBaseFactor", -1.0, 0.0, 0.05, [-0.3]);
            CosmoScout.gui.initSlider("crosshair.setRollAmplifier", 0.0, 3.0, 0.1, [1.0]);

            this.picker = document.querySelector('#crosshair-setColor');

            this.picker.picker = new CP(this.picker);
            this.picker.picker.self.classList.add('no-alpha');
            this.picker.picker.on('drag', (r, g, b, a) => {
                const color = CP.HEX([r, g, b, 1]);
                this.picker.style.background = color;
                this.picker.value = color;

                CosmoScout.callbacks.crosshair.setColor(color);
            });
            this.picker.oninput = (e) => {
                const color = CP.HEX(e.target.value);
                this.picker.picker.set(color[0], color[1], color[2], 1);
                this.picker.style.background = CP.HEX([color[0], color[1], color[2], 1]);

                CosmoScout.callbacks.crosshair.setColor(e.target.value);
            };
        }

        setColorValue(color) {
            this.picker.picker.set(CP.HEX(color));
            this.picker.style.background = color;
            this.picker.value = color;
        }
    }

    /**
     * ////////////////////////////////////////////////////////////////////////////////////////////////////
     * Floor Grid Api
     */
    class FloorGridApi extends IApi {
        /**
         * @inheritDoc
         */
        name = 'floorGrid';
        picker;

        /**
         * @inheritDoc
         */
        init() {
            CosmoScout.gui.initSlider("floorGrid.setSize", 0.1, 2, 0.1, [0.5]);
            CosmoScout.gui.initSlider("floorGrid.setExtent", 1, 20, 0.1, [10]);
            CosmoScout.gui.initSlider("floorGrid.setAlpha", 0, 1, 0.01, [1]);
            this.picker = document.querySelector('#floorGrid-setColor');

            this.picker.picker = new CP(this.picker);
            this.picker.picker.self.classList.add('no-alpha');
            this.picker.picker.on('drag', (r, g, b, a) => {
                const color = CP.HEX([r, g, b, 1]);
                this.picker.style.background = color;
                this.picker.value = color;

                CosmoScout.callbacks.floorGrid.setColor(color);
            });
            this.picker.oninput = (e) => {
                const color = CP.HEX(e.target.value);
                this.picker.picker.set(color[0], color[1], color[2], 1);
                this.picker.style.background = CP.HEX([color[0], color[1], color[2], 1]);

                CosmoScout.callbacks.floorGrid.setColor(e.target.value);
            };
        }

        setColorValue(color) {
            this.picker.picker.set(CP.HEX(color));
            this.picker.style.background = color;
            this.picker.value = color;
        }
    }

    /**
     * ////////////////////////////////////////////////////////////////////////////////////////////////////
     * FoV Vignette Api
     */
    class FovVignetteApi extends IApi {
        /**
         * @inheritDoc
         */
        name = 'fovVignette';
        picker;

        /**
         * @inheritDoc
         */
        init() {
            CosmoScout.gui.initSlider("fovVignette.setRadii", 0, 1.5, 0.01, [0.5, 1.0]);
            CosmoScout.gui.initSlider("fovVignette.setVelocityThresholds", 0, 15, 0.1, [0.2, 10.0]);
            CosmoScout.gui.initSlider("fovVignette.setDuration", 0.1, 2, 0.01, [1.0]);
            CosmoScout.gui.initSlider("fovVignette.setDeadzone", 0, 1, 0.01, [0.5]);
            this.picker = document.querySelector('#fovVignette-setColor');

            this.picker.picker = new CP(this.picker);
            this.picker.picker.self.classList.add('no-alpha');
            this.picker.picker.on('change', (r, g, b, a) => {
                const color = CP.HEX([r, g, b, 1]);
                this.picker.style.background = color;
                this.picker.value = color;
                CosmoScout.callbacks.fovVignette.setColor(color);
            });

            this.picker.oninput = (e) => {
                const color = CP.HEX(e.target.value);
                this.picker.picker.set(color[0], color[1], color[2], 1);
                this.picker.style.background = CP.HEX([color[0], color[1], color[2], 1]);
                CosmoScout.callbacks.fovVignette.setColor(color);
            }
        }

        setColorValue(color) {
            this.picker.picker.set(CP.HEX(color));
            this.picker.style.background = color;
            this.picker.value = color;
        }
    }
    /**
     * ////////////////////////////////////////////////////////////////////////////////////////////////////
     * INITIALIZATION
     */
    CosmoScout.init(
        VirtualHorizonApi,
        CrosshairApi,
        FloorGridApi,
        MotionPointsApi,
        FovVignetteApi,
        ViewOffsetApi
    );
})();

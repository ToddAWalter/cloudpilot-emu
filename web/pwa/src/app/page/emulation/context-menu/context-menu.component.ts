import { ActionSheetController, AlertController, ModalController, PopoverController } from '@ionic/angular';
import { Component, Input } from '@angular/core';
import { quirkNoPoweroff, slotType } from '@common/helper/deviceProperties';

import { AlertService } from '@pwa/service/alert.service';
import { AudioService } from '@pwa/service/audio.service';
import { BackupService } from '@pwa/service/backup.service';
import { ButtonService } from '@pwa/service/button.service';
import { CanvasDisplayService } from '@pwa/service/canvas-display.service';
import { CloudpilotService } from '@pwa/service/cloudpilot.service';
import { DeviceOrientation } from '@common/model/DeviceOrientation';
import { EmulationService } from '@pwa/service/emulation.service';
import { EmulationStateService } from '@pwa/service/emulation-state.service';
import { ErrorService } from '@pwa/service/error.service';
import { KvsService } from '@pwa/service/kvs.service';
import { PalmButton } from '@common/bridge/Cloudpilot';
import { PerformanceWatchdogService } from '@pwa/service/performance-watchdog.service';
import { SessionService } from '@pwa/service/session.service';
import { SessionSettingsComponent } from '@pwa/component/session-settings/session-settings.component';
import { SlotType } from '@common/model/SlotType';
import { StorageCardService } from '@pwa/service/storage-card.service';
import { StorageService } from '@pwa/service/storage.service';

function rotate(oldOrientation: DeviceOrientation | undefined): DeviceOrientation {
    switch (oldOrientation) {
        case DeviceOrientation.portrait:
        case undefined:
            return DeviceOrientation.landscape90;

        case DeviceOrientation.landscape90:
            return DeviceOrientation.portrait180;

        case DeviceOrientation.portrait180:
            return DeviceOrientation.landscape270;

        case DeviceOrientation.landscape270:
            return DeviceOrientation.portrait;

        default:
            throw new Error('cannot happen');
    }
}

@Component({
    selector: 'app-emulation-context-menu',
    templateUrl: './context-menu.component.html',
    styleUrls: ['./context-menu.component.scss'],
    standalone: false
})
export class ContextMenuComponent {
    constructor(
        public emulationService: EmulationService,
        private emulationStateService: EmulationStateService,
        private backupService: BackupService,
        private buttonService: ButtonService,
        private actionSheetController: ActionSheetController,
        private popoverController: PopoverController,
        private audioService: AudioService,
        private kvsService: KvsService,
        private modalController: ModalController,
        private sessionService: SessionService,
        private performanceWatchdogService: PerformanceWatchdogService,
        private canvasDisplayService: CanvasDisplayService,
        private cloudpilotService: CloudpilotService,
        private storageCardService: StorageCardService,
        private alertService: AlertService,
        private alertController: AlertController,
        private storageService: StorageService,
        private errorService: ErrorService,
    ) {}

    async reset(): Promise<void> {
        void this.popoverController.dismiss();

        const sheet = await this.actionSheetController.create({
            header: 'How do you want to reset?',
            buttons: [
                {
                    text: 'Normal boot',
                    handler: () => this.emulationService.reset(),
                },
                {
                    text: 'No extensions',
                    handler: () => this.emulationService.resetNoExtensions(),
                },
                {
                    text: 'Hard reset',
                    handler: () => this.emulationService.resetHard(),
                },
                {
                    text: 'Cancel',
                    role: 'cancel',
                },
            ],
        });

        void sheet.present();
    }

    power(): void {
        this.buttonService.engage(PalmButton.power);

        void this.popoverController.dismiss();
    }

    hotsync(): void {
        this.buttonService.engage(PalmButton.cradle);

        void this.popoverController.dismiss();
    }

    help(): void {
        this.showHelp();

        void this.popoverController.dismiss();
    }

    async backup(): Promise<void> {
        void this.popoverController.dismiss();

        const sheet = await this.actionSheetController.create({
            header: 'What do you want to backup?',
            buttons: [
                {
                    text: 'Only RAM (default)',
                    handler: () => this.backupService.saveBackup(false),
                },
                {
                    text: 'Include ROM',
                    handler: () => this.backupService.saveBackup(true),
                },
                {
                    text: 'Cancel',
                    role: 'cancel',
                },
            ],
        });

        void sheet.present();
    }

    get isMuted(): boolean {
        return !this.audioService.isInitialized() || this.audioService.isMuted();
    }

    get isAudioOff(): boolean {
        return this.kvsService.kvs.volume <= 0;
    }

    mute(muted: boolean): void {
        if (muted) {
            this.audioService.mute(true);
        } else {
            if (this.audioService.isInitialized()) {
                this.audioService.mute(false);
            } else {
                void this.audioService.initialize();
            }
        }
    }

    get backupDisabled(): boolean {
        return (
            !this.emulationService.isRunning() ||
            this.emulationService.isPowerOff() ||
            !this.emulationService.isUiInitialized()
        );
    }

    get powerDisabled(): boolean {
        const currentSession = this.emulationStateService.getCurrentSession();

        return currentSession ? quirkNoPoweroff(currentSession.device) : false;
    }

    get hasActiveSession(): boolean {
        return !!this.emulationStateService.getCurrentSession();
    }

    get supportsStorageCard(): boolean {
        const currentSession = this.emulationStateService.getCurrentSession();
        if (!currentSession) return false;

        return slotType(currentSession.device) !== SlotType.none;
    }

    get hasMemorystick(): boolean {
        const currentSession = this.emulationStateService.getCurrentSession();
        if (!currentSession) return false;

        return slotType(currentSession.device) === SlotType.memorystick;
    }

    get hasMountedCard(): boolean {
        return this.emulationStateService.getCurrentSession()?.mountedCard !== undefined;
    }

    async editSettings(): Promise<void> {
        const session = this.emulationStateService.getCurrentSession();
        if (!session) {
            return;
        }

        const oldSpeed = session.speed;
        const oldOrientation = session.deviceOrientation;

        const modal = await this.modalController.create({
            component: SessionSettingsComponent,
            backdropDismiss: false,
            componentProps: {
                session,
                availableDevices: [session.device],
                onSave: async () => {
                    if (oldSpeed !== session.speed) this.performanceWatchdogService.reset();
                    if (oldOrientation !== session.deviceOrientation) {
                        void this.canvasDisplayService.initialize(undefined, session);
                        this.canvasDisplayService.updateEmulationCanvas();
                    }

                    await this.sessionService.updateSession(session);

                    void modal.dismiss();
                },
                onCancel: () => modal.dismiss(),
            },
        });

        await this.popoverController.dismiss();
        await modal.present();
    }

    async rotate(): Promise<void> {
        const oldSession = this.emulationStateService.getCurrentSession();
        if (!oldSession) return;

        await this.storageService.updateSessionPartial(oldSession.id, {
            deviceOrientation: rotate(oldSession.deviceOrientation),
        });

        const session = this.emulationStateService.getCurrentSession();

        await this.canvasDisplayService.initialize(undefined, session);
        this.canvasDisplayService.updateEmulationCanvas();
    }

    async insertCard(): Promise<void> {
        void this.popoverController.dismiss();

        const deviceId = this.emulationStateService.getCurrentSession()?.device;
        if (deviceId === undefined) return;

        const cloudpilot = await this.cloudpilotService.cloudpilot;
        const eligibleCards = this.storageCardService
            .getAllCards()
            .filter(
                (card) =>
                    cloudpilot.deviceSupportsCardSize(card.size) && !this.storageCardService.mountedInSession(card.id),
            );

        if (eligibleCards.length === 0) {
            void this.alertService.message(
                'No image available',
                `There is no ${
                    this.hasMemorystick ? 'memory stick' : 'SD card'
                } image available. Please go to the "Storage" tab in order to create new one.`,
            );

            return;
        }

        const alert = await this.alertController.create({
            header: 'Choose image for insertion',
            buttons: [
                { text: 'Cancel', role: 'cancel' },
                { text: 'Insert', handler: (id) => this.storageCardService.insertCard(id) },
            ],
            inputs: eligibleCards.map((card, index) => ({
                label: card.name,
                type: 'radio',
                value: card.id,
                checked: index === 0,
            })),
            backdropDismiss: false,
        });

        await alert.present();
    }

    async ejectCard(): Promise<void> {
        try {
            await this.storageCardService.ejectCurrentCard();
        } catch (e) {
            this.errorService.fatalBug(e instanceof Error ? e.message : 'eject failed');
        }
        void this.popoverController.dismiss();
    }

    @Input()
    showHelp: () => void = () => undefined;
}

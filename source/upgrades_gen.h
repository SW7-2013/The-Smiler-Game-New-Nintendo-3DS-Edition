// generated from the original game data
#pragma once
typedef struct { const char* name; const char* display; const char* desc; int shop, maxLevel, maxUnlocked; int costs[3]; } UpgradeDef;
#define N_UPGRADES 15
#define N_TRACK_UPGRADES 10
static const UpgradeDef UPGRADES[N_UPGRADES] = {
	{ "Loop1", "Indoor Heartline", "The darkest inversion of all", 0, 1, 2, { 15, 0, 0 } },
	{ "Loop2", "Inverted Drop", "Why not do the first drop upside down?", 0, 1, 2, { 28, 0, 0 } },
	{ "Loop3", "Double Butterfly", "Two massive Butterfly loops for your pleasure", 0, 1, 0, { 50, 0, 0 } },
	{ "Loop4", "Batwing x2", "Because one Bat Wing is never enough", 0, 1, 2, { 62, 0, 0 } },
	{ "Loop5", "Camelback to Inline", "Who doesn't love camels?!", 0, 1, 2, { 25, 0, 0 } },
	{ "Loop6", "Twisted Drop", "Fall from the sky while flipping upside down", 0, 1, 2, { 36, 0, 0 } },
	{ "Loop7", "Pretzel + Dive Loops", "Terrifyingly delightful", 0, 1, 2, { 65, 0, 0 } },
	{ "Loop8", "Cobra Roll", "Because everyone likes Cobras", 0, 1, 2, { 46, 0, 0 } },
	{ "Loop9", "HeartLine Spin", "Spin your heart and welcome to joy", 0, 1, 0, { 50, 0, 0 } },
	{ "Loop10", "Last Twist", "Go on one last inversion before the end", 0, 1, 2, { 15, 0, 0 } },
	{ "GasLeg", "The Giggler", "Infectious, intoxicating laughing gas", 1, 3, 2, { 12, 16, 30 } },
	{ "HypnoLeg", "The Hypnotiser", "Has the power to disorientate, mesmerise and disrupt your self-awareness.", 1, 3, 2, { 12, 16, 30 } },
	{ "FlashLeg", "The Flasher", "A giant flashing device, blinding you as you hurtle underneath the leg.", 1, 3, 2, { 10, 16, 30 } },
	{ "TickleLeg", "The Tickler", "Aims to tickle you until you can't resist smiling", 1, 3, 2, { 12, 14, 30 } },
	{ "InocLeg", "The Inoculator", "A jab of happiness as you pass by stage one of the marmalisation process", 1, 3, 2, { 12, 14, 30 } },
};

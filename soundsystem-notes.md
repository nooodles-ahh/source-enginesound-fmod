# Notes
Misc notes for my own benefit

# Channels
CHAN_REPLACE	= -1,
CHAN_AUTO		= 0,
CHAN_WEAPON		= 1,
CHAN_VOICE		= 2,
CHAN_ITEM		= 3,
CHAN_BODY		= 4,
CHAN_STREAM		= 5,		- allocate stream channel from the static or dynamic area
CHAN_STATIC		= 6,		- allocate channel from the static area 
CHAN_VOICE2		= 7,
CHAN_VOICE_BASE	= 8,		- allocate channel for network voice data

## CHAN_REPLACE	= -1
    Used when playing sounds through console commands.
## CHAN_AUTO		= 0
    Default, generic channel.
## CHAN_WEAPON		= 1
    Player and NPC weapons.
	- Channel stealing
## CHAN_VOICE		= 2
    Voiceover dialogue. Used for regular voice lines, etc.
    Note:
    If sound with this channel is emit by an entity that has the "mouth" attachment, the sound comes from it.
	- Affected by pause
	- Channel stealing
## CHAN_ITEM		= 3
    Generic physics impact sounds, health/suit chargers, +use sounds.
## CHAN_BODY		= 4
    Clothing, ragdoll impacts, footsteps, knocking/pounding/punching etc.
## CHAN_STREAM		= 5
    Sounds that can be delayed by an async load, i.e. aren't responses to particular events.
    Confirm:This won't make the sound actually stream; use the * prefix for that.
## CHAN_STATIC		= 6
    A constant/background sound that doesn't require any reaction.
## CHAN_VOICE2		= 7
    Additional voice channel. Used in TF2 for the announcer.
	- Affected by pause
	- Channel stealing
## CHAN_VOICE_BASE = 8
    Network voice data (online voice communications)
	

# Channel stealing
On the channels that it occurs on, there appears to be a sound duration for when a sound gets overridden

# ADSP
Auto DSP is how source alters the DSP based on the size and characteristics of the space the player is in.
`adsp_debug` reveals a lot on how it works.

- Only runs while on/near the ground
- Considers the amount of sky visible

## Room types
- carpet hallway
- tile hallway
- wood hallway
- metal hallway

- train tunnel
- sewer main tunnel
- concrete access tunnel
- cave tunnel
- sand floor cave tunnel

- metal duct shaft
- elevator shaft
- large elevator shaft

- parking garage
- aircraft hangar
- cathedral
- train station

- small cavern
- large cavern
- huge cavern
- watery cavern
- long, low cavern

- wood warehouse
- metal warehouse
- concrete warehouse

- small closet room
- medium drywall room
- medium wood room
- medium metal room

- elevator
- small metal room
- medium metal room
- large metal room
- huge metal room

- small metal room dense
- medium metal room dense
- large metal room dense
- huge metal room dense

- small concrete room
- medium concrete room
- large concrete room
- huge concrete room

- small concrete room dense
- medium concrete room dense
- large concrete room dense
- huge concrete room dense

- soundproof room
- carpet lobby
- swimming pool
- open park
- open courtyard
- wide parkinglot
- narrow street
- wide street, short buildings
- wide street, tall buildings
- narrow canyon
- wide canyon
- huge canyon
- small valley
- wide valley
- wreckage & rubble
- small building cluster
- wide open plain
- high vista

- alien interior small
- alien interior medium
- alien interior large
- alien interior huge

## Convars
- "adsp_debug" = "0"
- "adsp_door_height" = "112"
- "adsp_wall_height" = "128"
- "adsp_low_ceiling" = "108"
- "adsp_room_min" = "102"
- "adsp_duct_min" = "106"
- "adsp_hall_min" = "110"
- "adsp_tunnel_min" = "114"
- "adsp_street_min" = "118"
- "adsp_alley_min" = "122"
- "adsp_courtyard_min" = "126"
- "adsp_openspace_min" = "130"
- "adsp_openwall_min" = "130"
- "adsp_openstreet_min" = "118"
- "adsp_opencourtyard_min" = "126"


## Describing a space
Size/materials should impact amount of reverb

### Room
Room ratio < 2.5, Sky = 0

### Tunnel
Room ratio >= 4, Sky = 0

### Hall
Room factor >= 2.5, ShortSide <= 96

### Street
Room ratio >=2.5, ShortSide > 144, Sky = 1

### Alley
Room ratio >= 3, ShortSide >= 96 && <= 144, Sky = 1

### Courtyard
Room ratio < 2.5 and walls > adsp_wall_height 

### Open space
Room ratio < 2.5 and walls < adsp_wall_height 

### Open wall
TODO

### Open street
TODO

### Open courtyard
TODO
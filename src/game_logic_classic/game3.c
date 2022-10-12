/* Copyright (C) 2022, Nikolai Wuttke. All rights reserved.
 *
 * This project is based on disassembly of NUKEM2.EXE from the game
 * Duke Nukem II, Copyright (C) 1993 Apogee Software, Ltd.
 *
 * Some parts of the code are based on or have been adapted from the Cosmore
 * project, Copyright (c) 2020-2022 Scott Smitelli.
 * See LICENSE_Cosmore file at the root of the repository, or refer to
 * https://github.com/smitelli/cosmore/blob/master/LICENSE.
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


/*******************************************************************************

Game logic, part 3: Actor system, collision response logic, utility functions

This file contains the implementation of the actor system - the "engine" for
the game logic - and a fairly big part of the game logic itself in the form of
"collision response" code. This code defines what should happen when an actor
is hit by a player shot and/or when it touches the player. Unlike the per-frame
update logic, which is implemented as invidivual functions for each actor,
the collision response is organized into switch statements which have dedicated
logic for specific actor IDs.

Various helper functions which are used to implement actor-specific logic are
also found here.

*******************************************************************************/

/** Initialize actor state at given list index, based on the parameters */
void pascal InitActorState(
  word listIndex,
  ActorUpdateFunc updateFunc,
  word id,
  word x,
  word y,
  word alwaysUpdate,
  word remainActive,
  word allowStairStepping,
  word gravityAffected,
  word health,
  word var1,
  word var2,
  word var3,
  word var4,
  word var5,
  word scoreGiven)
{
  ActorState* actor = gmActorStates + listIndex;

  actor->id = id;
  actor->frame = 0;
  actor->x = x;
  actor->y = y;
  actor->alwaysUpdate = alwaysUpdate;
  actor->remainActive = remainActive;
  actor->allowStairStepping = allowStairStepping;
  actor->gravityAffected = gravityAffected;
  actor->deleted = false;
  actor->gravityState = 0;
  actor->var1 = var1;
  actor->var2 = var2;
  actor->var3 = var3;
  actor->var4 = var4;
  actor->var5 = var5;
  actor->health = health;
  actor->scoreGiven = scoreGiven;
  actor->drawStyle = DS_NORMAL;
  actor->updateFunc = updateFunc;
}


/** Test if given actor is hit by a shot and return amount of damage if so
 *
 * This function tests if the given actor is intersecting with any player
 * shot or fire bomb fire. If it is, the appropriate amount of damage is
 * returned, 0 otherwise. Player shots which do not pass through enemies
 * are also marked by this function, to be deleted next frame in
 * UpdateAndDrawPlayerShots().
 *
 * In addition to the return value, a second value is returned via the global
 * variable retPlayerShotDirection, which indicates which direction the
 * player shot was moving into (for horizontal shots only).
 *
 * Since the collision detection works by going through the list of actors
 * and testing each actor against all shots, an actor can only ever be hit
 * by one shot each frame, but a single shot can cause damage to multiple
 * actors.
 */
byte pascal TestShotCollision(word handle)
{
  PlayerShot* shot;
  ActorState* actor = gmActorStates + handle;
  word i;

  // The player can't be hit by their own shots
  if (actor->id == ACT_DUKE_L || actor->id == ACT_DUKE_R)
  {
    return 0;
  }

  // Test fire bomb fires
  for (i = 0; i < MAX_NUM_EFFECTS; i++)
  {
    // [PERF] This is inefficient due to the intersection test being the
    // first condition. This loop is always doing intersection tests for
    // 18 effects, no matter how many effects are actually present.
    // Swapping the conditions to put the intersection test last would
    // make this much more efficient due to short-circuiting.
    if (
      AreSpritesTouching(
        actor->id,
        actor->frame,
        actor->x,
        actor->y,
        ACT_FIRE_BOMB_FIRE,
        gmEffectStates[i].active - 1,
        gmEffectStates[i].x,
        gmEffectStates[i].y) &&
      gmEffectStates[i].active && gmEffectStates[i].id == ACT_FIRE_BOMB_FIRE &&
      gmEffectStates[i].spawnDelay <= 1)
    {
      return 1;
    }
  }

  // Test player shots
  for (i = 0; i < MAX_NUM_PLAYER_SHOTS; i++)
  {
    if (gmPlayerShotStates[i].active == 0)
    {
      continue;
    }

    shot = gmPlayerShotStates + i;

    if (AreSpritesTouching(
          actor->id,
          actor->frame,
          actor->x,
          actor->y,
          shot->id,
          shot->active - 1,
          shot->x,
          shot->y))
    {
      retPlayerShotDirection = shot->direction;

      switch (shot->id)
      {
        case ACT_REGULAR_SHOT_HORIZONTAL:
        case ACT_REGULAR_SHOT_VERTICAL:
          shot->active = shot->active | 0x8000; // deactivate shot
          return WPN_DAMAGE_REGULAR;

        case ACT_DUKE_LASER_SHOT_HORIZONTAL:
        case ACT_DUKE_LASER_SHOT_VERTICAL:
          return WPN_DAMAGE_LASER;

        case ACT_DUKE_FLAME_SHOT_UP:
        case ACT_DUKE_FLAME_SHOT_DOWN:
        case ACT_DUKE_FLAME_SHOT_LEFT:
        case ACT_DUKE_FLAME_SHOT_RIGHT:
          shot->active = shot->active | 0x8000; // deactivate shot
          return WPN_DAMAGE_FLAME_THROWER;

        case ACT_DUKE_ROCKET_UP:
        case ACT_DUKE_ROCKET_DOWN:
        case ACT_DUKE_ROCKET_LEFT:
        case ACT_DUKE_ROCKET_RIGHT:
          shot->active = shot->active | 0x8000; // deactivate shot
          SpawnEffect(ACT_EXPLOSION_FX_2, shot->x - 3, shot->y + 3, EM_NONE, 0);
          return WPN_DAMAGE_ROCKET_LAUNCHER;

        case ACT_REACTOR_FIRE_L:
        case ACT_REACTOR_FIRE_R:
        case ACT_DUKES_SHIP_LASER_SHOT:
          SpawnEffect(ACT_EXPLOSION_FX_2, shot->x - 3, shot->y + 3, EM_NONE, 0);
          return WPN_DAMAGE_SHIP_LASER;
      }
    }
  }

  return 0;
}


/** Test if sprite's bounding box is intersecting specified rectangle */
bool pascal IsSpriteInRect(
  word id,
  word x,
  word y,
  word left,
  word top,
  word right,
  word bottom)
{
  register word height;
  register word width;
  register word rectHeight;
  register word rectWidth;
  word offset;

  offset = gfxActorInfoData[id];
  x += AINFO_X_OFFSET(offset);
  y += AINFO_Y_OFFSET(offset);
  height = AINFO_HEIGHT(offset);
  width = AINFO_WIDTH(offset);
  rectHeight = bottom - top;
  rectWidth = right - left;

  if (
    ((left <= x && left + rectWidth > x) || (left >= x && x + width > left)) &&
    ((y - height < bottom && bottom <= y) ||
     (bottom - rectHeight < y && y <= bottom)))
  {
    return true;
  }
  else
  {
    return false;
  }
}


/** Test if a player shot intersects the given rectangle, delete it if so */
bool pascal FindPlayerShotInRect(word left, word top, word right, word bottom)
{
  PlayerShot* shot;
  word i;

  for (i = 0; i < MAX_NUM_PLAYER_SHOTS; i++)
  {
    if (gmPlayerShotStates[i].active)
    {
      shot = gmPlayerShotStates + i;

      if (IsSpriteInRect(shot->id, shot->x, shot->y, left, top, right, bottom))
      {
        shot->active = 0; // delete the shot
        return true;
      }
    }
  }

  return false;
}


/** Try unlocking a key card slot or key hole actor */
void pascal TryUnlockingDoor(bool* pSuccess, word neededKeyId, word handle)
{
  ActorState* actor = gmActorStates + handle;

  if (actor->var1) // door not unlocked yet?
  {
    plBlockLookingUp = true;

    if (inputMoveUp && RemoveFromInventory(neededKeyId))
    {
      *pSuccess = true;

      // Let the player show the "interact" animation
      plInteractAnimTicks = 1;

      // Mark lock as unlocked
      actor->var1 = false;
    }
  }
  else
  {
    plBlockLookingUp = false;
  }
}


/** Check if the player has collected all letters, but in the wrong order */
void CheckLetterCollectionPityBonus(void)
{
  // [BUG] `plCollectedLetters != 5` is always true, if the first condition is
  // true. The author most likely intended to compare only the low byte of
  // plCollectedLetters to 5. But since this function isn't called when
  // collecting all the letters in order, the bug doesn't materialize.
  if ((plCollectedLetters >> 8 & 0x1F) == 0x1F && plCollectedLetters != 5)
  {
    ShowInGameMessage(
      "OH, WELL... TEN THOUSAND POINTS IS*BETTER THAN NOTHING.");
  }

  // [BUG] The 10k points are given for each letter that's collected, instead of
  // only when collecting all letters out of order. This seems like a bug, since
  // the message about getting 10k points is only shown upon collecting the last
  // letter, and collecting a letter spanws a floating 100 but gives 10100
  // points.
  // My theory is that the if statement above didn't have curly braces in
  // the original code. Originally, the if statement body only contained the
  // GiveScore() call. The message was then added later in development without
  // adding curly braces to the if statement, leading to this bug.
  GiveScore(10000);
}


/** Convenience helper function */
bool pascal Boss3_IsTouchingPlayer(word handle)
{
  ActorState* actor = gmActorStates + handle;

  return AreSpritesTouching(
    actor->id,
    actor->frame,
    actor->x,
    actor->y,
    plActorId,
    plAnimationFrame,
    plPosX,
    plPosY);
}


/** Handle actors touching the player
 *
 * The primary job of this function is to cause damage to the player when
 * touching enemies, and to handle picking up collectible items.
 * Basically, this function defines what happens for each type of actor when
 * that actor touches the player. It also performs the collision detection,
 * by testing if the given actor's sprite intersects the player's.
 *
 * [NOTE] A lot of the code here could've been avoided if the actor system had
 * been extended to feature a "damages player" flag that actors could set on
 * themselves, which would then be handled in UpdateAndDrawActors().
 */
void pascal UpdateActorPlayerCollision(word handle)
{
  ActorState* state = gmActorStates + handle;
  int i;

  if (plState == PS_DYING)
  {
    return;
  }
  if (plState == PS_GETTING_EATEN)
  {
    return;
  }

  if (AreSpritesTouching(
        state->id,
        state->frame,
        state->x,
        state->y,
        plActorId,
        plAnimationFrame,
        plPosX,
        plPosY))
  {
    switch (state->id)
    {
      case ACT_AGGRESSIVE_PRISONER:
        // Only damage the player if currently grabbing
        if (state->var1 == true)
        {
          DamagePlayer();
        }
        break;

      case ACT_SUPER_FORCE_FIELD_L:
        if (plCloakTimeLeft)
        {
          // Player is cloaked, initiate the destruction sequence
          if (!state->var3)
          {
            state->var3 = 1;
          }

          break;
        }

        // Activate animation, handled in Act_SuperForceField()
        if (!state->var1)
        {
          state->var1 = 1;
        }

        // Prevent the player from passing through
        if (plPosX + 2 <= state->x)
        {
          plPosX--;
        }

        if (plPosX + 2 > state->x)
        {
          plPosX++;
        }

        DamagePlayer();
        ShowTutorial(
          TUT_CLOAK_NEEDED,
          "OUCH, YOU NEED TO FIND THE CLOAKING*DEVICE TO DISABLE THIS FORCEFIELD.");
        break;

      case ACT_RESPAWN_CHECKPOINT:
        if (!state->var3 && state->frame == 0)
        {
          state->var3 = 15;
        }
        break;

      case ACT_CIRCUIT_CARD_KEYHOLE:
        if (plPosY - 2 == state->y)
        {
          TryUnlockingDoor(
            &gmRequestUnlockNextForceField, ACT_CIRCUIT_CARD, handle);
          ShowTutorial(
            TUT_FOUND_FORCE_FIELD,
            "USE THE ACCESS CARD TO DISABLE*THIS FORCE FIELD.");

          if (inputMoveUp)
          {
            if (state->var1)
            {
              ShowTutorial(TUT_CARD_NEEDED, "ACCESS DENIED.");
              plBlockLookingUp = false;
            }
            else if (gmRequestUnlockNextForceField)
            {
              ShowInGameMessage("ACCESS GRANTED.");
            }
          }
        }
        break;

      case ACT_BLUE_KEY_KEYHOLE:
        if (plPosY - 2 == state->y)
        {
          ShowTutorial(TUT_FOUND_KEYHOLE, "USE A KEY TO OPEN THIS DOOR.");
          TryUnlockingDoor(&gmRequestUnlockNextDoor, ACT_BLUE_KEY, handle);

          if (inputMoveUp)
          {
            if (state->var1)
            {
              ShowTutorial(TUT_KEY_NEEDED, "YOU NEED A KEY TO OPEN*THE DOOR.");
              plBlockLookingUp = false;
            }
            else if (gmRequestUnlockNextDoor)
            {
              ShowInGameMessage("OPENING DOOR.");
            }
          }
        }
        break;

      case ACT_DUKES_SHIP_R:
      case ACT_DUKES_SHIP_L:
        if (
          plState == PS_FALLING &&
          state->var1 == 0 && // ship pickup cooldown has elapsed
          state->gravityState == 0) // ship is on solid ground
        {
          ShowTutorial(
            TUT_SHIP,
            "THE SHIP. USE THIS TO GET OUT OF THIS*LEVEL.  YOU CAN FLY ANYWHERE, AND*YOUR WEAPON IS VERY POWERFUL.");

          plState = PS_USING_SHIP;
          plActorId = state->id;
          plAnimationFrame = 1;
          plPosY = state->y;
          plPosX = state->x;

          PlaySound(SND_WEAPON_PICKUP);

          // Delete the pickup. Exiting the ship will respawn it.
          state->deleted = true;
        }
        break;

      case ACT_CEILING_SUCKER:
        if (
          plState != PS_USING_SHIP && state->frame == 5 && state->var1 < 10 &&
          plPosX + 1 >= state->x && state->x + 1 >= plPosX)
        {
          gmPlayerEatingActor = state->id;
          plState = PS_GETTING_EATEN;
          state->var2 = 1;
        }
        break;

      case ACT_HOVERBOT:
      case ACT_BIG_GREEN_CAT_L:
      case ACT_BIG_GREEN_CAT_R:
      case ACT_FLAME_THROWER_BOT_R:
      case ACT_FLAME_THROWER_BOT_L:
      case ACT_WATCHBOT:
      case ACT_ROCKET_LAUNCHER_TURRET:
      case ACT_WATCHBOT_CONTAINER_CARRIER:
      case ACT_MINI_NUKE:
      case ACT_BOUNCING_SPIKE_BALL:
      case ACT_SLIME_BLOB:
      case ACT_SLIME_BLOB_2:
      case ACT_WALL_WALKER:
      case ACT_SLIME_DROP:
      case ACT_SKELETON:
      case ACT_HOVERING_LASER_TURRET:
      case ACT_BLUE_GUARD_R:
      case ACT_UGLY_GREEN_BIRD:
      case ACT_ROTATING_FLOOR_SPIKES:
      case ACT_GREEN_CREATURE_L:
      case ACT_GREEN_CREATURE_R:
      case ACT_RED_BIRD:
      case ACT_LAVA_PIT:
      case ACT_GREEN_ACID_PIT:
      case ACT_UNICYCLE_BOT:
      case ACT_FIRE_ON_FLOOR_1:
      case ACT_FIRE_ON_FLOOR_2:
      case ACT_SMALL_FLYING_SHIP_1:
      case ACT_SMALL_FLYING_SHIP_2:
      case ACT_SMALL_FLYING_SHIP_3:
        DamagePlayer();
        // [BUG] Unintended fallthrough. No observable consequences, because
        // the player has invincibility frames after taking damage and so the
        // potential 2nd call to DamagePlayer() will have no effect.

      case ACT_BOSS_EPISODE_2:
      case ACT_BOSS_EPISODE_1:
      case ACT_BOSS_EPISODE_3:
      case ACT_BOSS_EPISODE_4:
        if (state->var3 < 2)
        {
          DamagePlayer();
        }
        break;

      case ACT_BOSS_EPISODE_4_SHOT:
        DamagePlayer();
        SpawnEffect(ACT_EXPLOSION_FX_1, state->x, state->y, EM_NONE, 0);
        state->deleted = true;
        break;

      case ACT_SPIDER:
        DamagePlayer();

        if (plCloakTimeLeft)
        {
          break;
        }

        if (
          (plAttachedSpider1 == 0 && state->gravityState != 0) ||
          ((plAttachedSpider2 == 0 || plAttachedSpider3 == 0) &&
           state->scoreGiven != 0 && // score field is repurposed as state
                                     // variable, indicating if the spider
                                     // is on the ground
           state->frame < 12))
        {
          if (!state->gravityState) // on ground
          {
            if (plAttachedSpider2)
            {
              plAttachedSpider3 = handle;
            }
            else
            {
              plAttachedSpider2 = handle;
            }
          }
          else if (plAttachedSpider1 == 0)
          {
            plAttachedSpider1 = handle;
          }

          state->health = 0; // make invincible
          state->var4 = 1; // mark as attached to player
          state->gravityAffected = false;
          state->gravityState = 0;
        }
        break;

      case ACT_SMASH_HAMMER:
        // Only damage player while smashing down
        if (state->var3 == 1)
        {
          DamagePlayer();
        }
        break;

      case ACT_EYEBALL_THROWER_L:
      case ACT_EYEBALL_THROWER_R:
        if (state->y - 5 < plPosY)
        {
          DamagePlayer();
        }
        break;

      case ACT_LASER_TURRET:
        // Only damage player if not currently spinning
        if (!state->var1)
        {
          DamagePlayer();
        }
        break;

      case ACT_ENEMY_LASER_SHOT_L:
        state->deleted = true;
        DamagePlayer();
        break;

      case ACT_SNAKE:
        if (
          // snake not currently eating player?
          !state->var2 && plState == PS_NORMAL)
        {
          if (state->var1) // snake facing right and player in reach? or...
          {
            if (
              (state->x + 3 == plPosX || state->x + 2 == plPosX) &&
              state->y == plPosY)
            {
              // Start eating the player (see Act_Snake)
              state->var2 = 1;
            }
          }
          else // ... snake facing left and player in reach?
          {
            if (
              (state->x - 3 == plPosX || state->x - 2 == plPosX) &&
              state->y == plPosY)
            {
              // Start eating the player (see Act_Snake)
              state->var2 = 1;
            }
          }
        }
        break;

      case ACT_ENEMY_ROCKET_LEFT:
      case ACT_ENEMY_ROCKET_UP:
      case ACT_ENEMY_ROCKET_RIGHT:
      case ACT_ENEMY_ROCKET_2_UP:
      case ACT_ENEMY_ROCKET_2_DOWN:
        DamagePlayer();
        SpawnEffect(ACT_EXPLOSION_FX_1, state->x, state->y, EM_NONE, 0);
        state->deleted = true;
        break;

      case ACT_ELECTRIC_REACTOR:
        // Insta-kill the player
        plHealth = 1;
        plMercyFramesLeft = 0;
        plCloakTimeLeft = 0;
        DamagePlayer();

        // [BUG] The cloak doesn't reappear if the player dies while cloaked
        // and then respawns at a checkpoint, potentially making the level
        // unwinnable.  This should use the same cloak respawning code here as
        // in Act_PlayerSprite().
        break;

      case ACT_NORMAL_WEAPON:
      case ACT_LASER:
      case ACT_FLAME_THROWER:
      case ACT_ROCKET_LAUNCHER:
        if (state->var1 > 8)
        {
          switch (state->id)
          {
            case ACT_NORMAL_WEAPON:
              ShowTutorial(TUT_WPN_REGULAR, "THIS IS YOUR REGULAR WEAPON!");
              break;

            case ACT_LASER:
              ShowTutorial(
                TUT_WPN_LASER, "THIS WEAPON CAN SHOOT*THROUGH ANYTHING!");
              break;

            case ACT_FLAME_THROWER:
              ShowTutorial(
                TUT_WPN_FLAMETHROWER,
                "THERE ARE MANY SECRETS WITH*THIS WEAPON. USE IT TO*YOUR ADVANTAGE!");
              break;

            case ACT_ROCKET_LAUNCHER:
              ShowTutorial(
                TUT_WPN_ROCKETLAUNCHER,
                "A ROCKET LAUNCHER IS VERY*DEADLY. IT IS THE STRONGEST OF*ANY WEAPON!");
              break;
          }

          if (state->id != ACT_FLAME_THROWER)
          {
            plAmmo = MAX_AMMO;
          }
          else
          {
            plAmmo = MAX_AMMO_FLAMETHROWER;
          }

          gmWeaponsCollected++;

          plWeapon = state->var3;

          PlaySound(SND_WEAPON_PICKUP);

          state->deleted = true;
          SpawnEffect(
            ACT_SCORE_NUMBER_FX_2000, state->x, state->y, EM_SCORE_NUMBER, 0);
          GiveScore(2000);
        }
        break;

      case ACT_SUNGLASSES:
        PlaySound(SND_ITEM_PICKUP);
        GiveScore(100);
        state->deleted = true;
        SpawnEffect(
          ACT_SCORE_NUMBER_FX_100, state->x, state->y, EM_SCORE_NUMBER, 0);
        gmMerchCollected++;
        break;

      case ACT_CAMERA:
        PlaySound(SND_ITEM_PICKUP);
        GiveScore(2500);
        state->deleted = true;
        SpawnEffect(
          ACT_SCORE_NUMBER_FX_2000, state->x, state->y - 1, EM_SCORE_NUMBER, 0);
        SpawnEffect(
          ACT_SCORE_NUMBER_FX_500, state->x, state->y, EM_SCORE_NUMBER, 0);
        gmMerchCollected++;
        break;

      case ACT_PHONE:
        PlaySound(SND_ITEM_PICKUP);
        GiveScore(2000);
        state->deleted = true;
        SpawnEffect(
          ACT_SCORE_NUMBER_FX_2000, state->x, state->y, EM_SCORE_NUMBER, 0);
        gmMerchCollected++;
        break;

      case ACT_TV:
        PlaySound(SND_ITEM_PICKUP);
        GiveScore(1500);
        state->deleted = true;
        SpawnEffect(
          ACT_SCORE_NUMBER_FX_500, state->x, state->y, EM_SCORE_NUMBER, 0);
        SpawnEffect(
          ACT_SCORE_NUMBER_FX_500, state->x, state->y - 1, EM_SCORE_NUMBER, 0);
        SpawnEffect(
          ACT_SCORE_NUMBER_FX_500, state->x, state->y - 2, EM_SCORE_NUMBER, 0);
        gmMerchCollected++;
        break;

      case ACT_BOOM_BOX:
        PlaySound(SND_ITEM_PICKUP);
        GiveScore(1000);
        state->deleted = true;
        SpawnEffect(
          ACT_SCORE_NUMBER_FX_500, state->x, state->y, EM_SCORE_NUMBER, 0);
        SpawnEffect(
          ACT_SCORE_NUMBER_FX_500, state->x, state->y - 1, EM_SCORE_NUMBER, 0);
        gmMerchCollected++;
        break;

      case ACT_GAME_CARTRIDGE:
      case ACT_DISK:
      case ACT_PC:
      case ACT_CD:
      case ACT_T_SHIRT:
      case ACT_VIDEOCASSETTE:
        PlaySound(SND_ITEM_PICKUP);

        if (state->id == ACT_T_SHIRT)
        {
          GiveScore(5000);
          SpawnEffect(
            ACT_SCORE_NUMBER_FX_5000,
            state->x,
            state->y - 2,
            EM_SCORE_NUMBER,
            0);
        }
        else
        {
          GiveScore(500);
          SpawnEffect(
            ACT_SCORE_NUMBER_FX_500, state->x, state->y, EM_SCORE_NUMBER, 0);
        }

        gmMerchCollected++;

        state->deleted = true;
        break;

      case ACT_TURKEY:
        PlaySound(SND_HEALTH_PICKUP);

        plHealth++;

        if (state->var2 == 2) // cooked turkey?
        {
          plHealth++;
        }

        if (plHealth > PLAYER_MAX_HEALTH)
        {
          plHealth = PLAYER_MAX_HEALTH;
        }

        // [BUG] The turkey doesn't give any points, but spawns a score number
        // on pickup
        SpawnEffect(
          ACT_SCORE_NUMBER_FX_100, state->x, state->y, EM_SCORE_NUMBER, 0);

        state->deleted = true;
        break;

      case ACT_SODA_CAN:
        state->deleted = true;

        if (state->var3) // soda can rocket
        {
          GiveScore(2000);
          PlaySound(SND_ITEM_PICKUP);
          SpawnEffect(
            ACT_SCORE_NUMBER_FX_2000, state->x, state->y, EM_SCORE_NUMBER, 0);
        }
        else
        {
          ShowTutorial(
            TUT_SODA, "THE CAFFEINE IN SODAS PROVIDES*ONE UNIT OF HEALTH.");
          GiveScore(100);
          PlaySound(SND_HEALTH_PICKUP);

          plHealth++;

          if (plHealth > PLAYER_MAX_HEALTH)
          {
            plHealth = PLAYER_MAX_HEALTH;
          }

          SpawnEffect(
            ACT_SCORE_NUMBER_FX_100, state->x, state->y, EM_SCORE_NUMBER, 0);
        }
        break;

      case ACT_SODA_6_PACK:
        PlaySound(SND_HEALTH_PICKUP);
        state->deleted = true;
        GiveScore(100);

        plHealth += 6;

        if (plHealth > PLAYER_MAX_HEALTH)
        {
          plHealth = PLAYER_MAX_HEALTH;
        }

        SpawnEffect(
          ACT_SCORE_NUMBER_FX_100, state->x, state->y, EM_SCORE_NUMBER, 0);
        break;

      case ACT_HEALTH_MOLECULE:
        // Only allow picking up the item if it has completed the upwards part
        // of the fly-up sequence after shooting the containing box
        if (state->var1 > 8)
        {
          ShowTutorial(
            TUT_HEALTH_MOLECULE, "THIS ITEM WILL GIVE YOU ONE UNIT*OF HEALTH.");
          PlaySound(SND_HEALTH_PICKUP);

          plHealth++;

          if (plHealth > PLAYER_MAX_HEALTH)
          {
            plHealth = PLAYER_MAX_HEALTH;

            GiveScore(10000);
            SpawnEffect(
              ACT_SCORE_NUMBER_FX_10000,
              state->x,
              state->y,
              EM_SCORE_NUMBER,
              0);
          }
          else
          {
            SpawnEffect(
              ACT_SCORE_NUMBER_FX_500, state->x, state->y, EM_SCORE_NUMBER, 0);

            GiveScore(500);
          }

          state->deleted = true;
        }
        break;

      case ACT_N:
        // The letter collection state is stored in plCollectedLetters. The
        // low byte of that value is the number of letters that have been
        // collected in the right order, while the high byte is a bitmask which
        // has one bit set for each letter that has been collected (regardless
        // of order).
        if (plCollectedLetters == 0)
        {
          plCollectedLetters++;

          ShowTutorial(TUT_N, "GOT THE N.  WHAT IS NEXT?");
        }

        plCollectedLetters |= 0x100;

        CheckLetterCollectionPityBonus();

        SpawnEffect(
          ACT_SCORE_NUMBER_FX_100, state->x, state->y, EM_SCORE_NUMBER, 0);
        GiveScore(100);

        PlaySound(SND_ITEM_PICKUP);

        state->deleted = true;
        break;

      case ACT_U:
        // See ACT_N above
        if ((plCollectedLetters & 7) == 1)
        {
          plCollectedLetters++;

          ShowTutorial(TUT_U, "GOT THE U.");
        }

        plCollectedLetters |= 0x200;

        CheckLetterCollectionPityBonus();

        SpawnEffect(
          ACT_SCORE_NUMBER_FX_100, state->x, state->y, EM_SCORE_NUMBER, 0);
        GiveScore(100);

        PlaySound(SND_ITEM_PICKUP);

        state->deleted = true;
        break;

      case ACT_K:
        // See ACT_N above
        if ((plCollectedLetters & 7) == 2)
        {
          plCollectedLetters++;

          ShowTutorial(TUT_K, "GOT THE K.");
        }

        plCollectedLetters |= 0x400;

        CheckLetterCollectionPityBonus();

        SpawnEffect(
          ACT_SCORE_NUMBER_FX_100, state->x, state->y, EM_SCORE_NUMBER, 0);
        GiveScore(100);

        PlaySound(SND_ITEM_PICKUP);

        state->deleted = true;
        break;

      case ACT_E:
        // See ACT_N above
        if ((plCollectedLetters & 7) == 3)
        {
          plCollectedLetters++;

          ShowTutorial(TUT_E, "GOT THE E.");
        }

        plCollectedLetters |= 0x800;

        CheckLetterCollectionPityBonus();

        SpawnEffect(
          ACT_SCORE_NUMBER_FX_100, state->x, state->y, EM_SCORE_NUMBER, 0);
        GiveScore(100);

        PlaySound(SND_ITEM_PICKUP);

        state->deleted = true;
        break;

      case ACT_M:
        plCollectedLetters |= 0x1000;

        // See ACT_N above
        if ((plCollectedLetters & 7) == 4)
        {
          byte i;

          sbyte SCORE_NUMBER_OFFSETS[] = {-3, 0, 3, 0};

          PlaySound(SND_LETTERS_COLLECTED_CORRECTLY);
          ShowTutorial(
            TUT_LETTERS_COLLECTED,
            "EXCELLENT!  ONE HUNDRED THOUSAND*POINTS!!!!!!!!!!!");

          for (i = 0; i < 10; i++)
          {
            SpawnEffect(
              ACT_SCORE_NUMBER_FX_10000,
              state->x + SCORE_NUMBER_OFFSETS[i & 3],
              state->y - i,
              EM_SCORE_NUMBER,
              0);
          }

          // GiveScore takes a 16-bit word, so we can't add 100000 in one go
          GiveScore(50000);
          GiveScore(50000);
        }
        else
        {
          CheckLetterCollectionPityBonus();

          SpawnEffect(
            ACT_SCORE_NUMBER_FX_100, state->x, state->y, EM_SCORE_NUMBER, 0);
          GiveScore(100);

          PlaySound(SND_ITEM_PICKUP);
        }

        state->deleted = true;
        break;

      case ACT_CLOAKING_DEVICE:
      case ACT_BLUE_KEY:
      case ACT_CIRCUIT_CARD:
        // Only allow picking up the item if it has completed the upwards part
        // of the fly-up sequence after shooting the containing box
        if (state->var1 <= 8)
        {
          break;
        }

        PlaySound(SND_ITEM_PICKUP);

        if (state->id == ACT_CLOAKING_DEVICE)
        {
          ShowInGameMessage(
            "YOU ARE NOW INVINCIBLE FOR A SHORT*PERIOD OF TIME...**USE THIS TO ALSO DISABLE*THE FORCE FIELD...");
          RemoveFromInventory(ACT_CLOAKING_DEVICE_ICON);
          AddInventoryItem(ACT_CLOAKING_DEVICE_ICON);
          plCloakTimeLeft = CLOAK_TIME;
          gmCloakPickupPosX = state->x;
          gmCloakPickupPosY = state->y;

          GiveScore(500);
          state->deleted = true;
        }
        else
        {
          if (state->id == ACT_BLUE_KEY)
          {
            ShowTutorial(TUT_KEY, "FIND THE DOOR THAT*THIS KEY OPENS.");
          }
          else
          {
            ShowTutorial(TUT_CARD, "USE THE CARD TO TURN OFF*FORCE FIELDS.");
          }

          SpawnEffect(
            ACT_SCORE_NUMBER_FX_500, state->x, state->y, EM_SCORE_NUMBER, 0);
          AddInventoryItem(state->id);
          GiveScore(500);

          state->deleted = true;
        }
        break;

      case ACT_RAPID_FIRE:
        // Only allow picking up the item if it has completed the upwards part
        // of the fly-up sequence after shooting the containing box
        if (state->var1 > 8)
        {
          PlaySound(SND_WEAPON_PICKUP);

          RemoveFromInventory(ACT_RAPID_FIRE_ICON);

          ShowTutorial(
            TUT_RAPID_FIRE, "HOLD DOWN YOUR FIRE BUTTON FOR*RAPID FIRE.");

          AddInventoryItem(ACT_RAPID_FIRE_ICON);

          SpawnEffect(
            ACT_SCORE_NUMBER_FX_500, state->x, state->y, EM_SCORE_NUMBER, 0);
          GiveScore(500);

          plRapidFireTimeLeft = RAPID_FIRE_TIME;
          state->deleted = true;
        }
        break;

      case ACT_SPECIAL_HINT_MACHINE:
        // If the globe has already been placed onto the hint machine, do
        // nothing
        if (state->var1)
        {
          break;
        }

        if (RemoveFromInventory(ACT_SPECIAL_HINT_GLOBE_ICON))
        {
          byte i;

          GiveScore(50000);
          PlaySound(SND_ITEM_PICKUP);

          for (i = 0; i < 5; i++)
          {
            SpawnEffect(
              ACT_SCORE_NUMBER_FX_10000,
              state->x,
              state->y - i,
              EM_SCORE_NUMBER,
              0);
          }

          // Mark the machine as having the globe placed
          state->var1 = true;

          // The scripting system also handles showing the hint machine
          // messages
          ShowScriptedUI("Hints", "HELP.MNI");
          hudShowingHintMachineMsg = true;
        }
        else
        {
          ShowTutorial(
            TUT_HINT_MACHINE,
            "THIS DEVICE WILL GIVE SPECIFIC HINTS.*FIND THE SPECIAL BLUE GLOBE AND*BRING IT BACK HERE.");
        }
        break;

      case ACT_SPECIAL_HINT_GLOBE:
        PlaySound(SND_ITEM_PICKUP);

        ShowInGameMessage(
          "BRING THIS SPECIAL CRYSTAL GLOBE*BACK TO THE PEDESTAL TO RECEIVE A*SPECIAL HINT FOR THIS LEVEL.");

        AddInventoryItem(ACT_SPECIAL_HINT_GLOBE_ICON);

        SpawnEffect(
          ACT_SCORE_NUMBER_FX_10000, state->x, state->y, EM_SCORE_NUMBER, 0);
        GiveScore(10000);

        state->deleted = true;
        break;

      case ACT_BONUS_GLOBE_SHELL:
        {
          i = ACT_SCORE_NUMBER_FX_500;

          GiveScore(state->scoreGiven);

          switch (state->scoreGiven)
          {
            case 2000:
              i += 1;
              break;

            case 5000:
              i += 2;
              break;

            case 10000:
              i += 3;
              break;
          }

          SpawnEffect(i, state->x, state->y, EM_SCORE_NUMBER, 0);

          PlaySound(SND_ITEM_PICKUP);

          state->deleted = true;
        }
        break;

      case ACT_TELEPORTER_2:
        if (
          state->x <= plPosX && state->x + 3 >= plPosX && state->y == plPosY &&
          plState == PS_NORMAL)
        {
          ShowTutorial(
            TUT_TELEPORTER, "PRESS UP OR ENTER TO USE*THE TRANSPORTER.*");
        }

        // Check if the player is interacting with the teleporter.
        // The active area is smaller than the sprite's bounding box, so we
        // need to do an additional check here even though we already called
        // AreSpritesTouching() at the top of this function.
        if (
          state->x <= plPosX && state->x + 3 >= plPosX && state->y == plPosY &&
          inputMoveUp && plState == PS_NORMAL)
        {
          byte counterpartId;
          ActorState* candidate;

          PlaySound(SND_TELEPORT);

          // The way the teleport target is found is based on the actor ID.
          // There are two actor IDs that both spawn a teleporter into the
          // level.  Each teleporter looks for the first actor in the list that
          // has an ID which is also a teleporter, but not the one the source
          // teleporter has. Since there is only one teleporter sprite, the
          // actual ID of the actor is set to the same for both, but the
          // original ID specified in the level file is stored in var2.
          //
          // One consequence of this design is that there can never be more
          // than one teleport destination in a level - it's not possible to
          // have two independent teleporter connections, for example.  Placing
          // more than two won't cause anything serious to happen, but the
          // result is unlikely to be what the level designer intends to happen.
          //
          // Here, we determine the right counterpart ID to use based on our
          // own ID, and also handle the "backdrop switch on teleport" logic
          // if enabled.
          if (state->var2 == ACT_TELEPORTER_1)
          {
            if (mapSwitchBackdropOnTeleport)
            {
              bdAddressAdjust = 0x4000;
            }

            counterpartId = ACT_TELEPORTER_2;
          }
          else
          {
            if (mapSwitchBackdropOnTeleport)
            {
              bdAddressAdjust = 0;
            }

            counterpartId = ACT_TELEPORTER_1;
          }

          // Now go through the entire list of actors, and find the first one
          // that is a) a teleporter and b) has the right counterpart ID.
          for (i = 0; i < gmNumActors; i++)
          {
            candidate = gmActorStates + i;

            if (
              counterpartId == candidate->var2 &&
              candidate->id == ACT_TELEPORTER_2)
            {
              // We have found our destination!

              // Clear any flying tile debris, since debris pieces don't take
              // the camera position into account and thus would suddenly appear
              // at the new location unless cleared.
              if (gmExplodingSectionTicksElapsed)
              {
                gmExplodingSectionTicksElapsed = 0;
              }

              TeleportTo(candidate->x, candidate->y);
              break;
            }
          }

          // We didn't find a suitable destination. If there's only one
          // teleporter in a level, it acts as level exit.
          if (i == gmNumActors)
          {
            gmGameState = GS_LEVEL_FINISHED;
          }
        }
        break;
    }
  }
}


/** Apply damage to actor. Return true if actor was killed, false otherwise */
bool pascal DamageActor(word damage, word handle)
{
  ActorState* actor = gmActorStates + handle;

  actor->health -= damage;
  actor->drawStyle = DS_WHITEFLASH;

  if (actor->health <= 0)
  {
    GiveScore(actor->scoreGiven);

    if (actor->id == gmPlayerEatingActor && plState == PS_GETTING_EATEN)
    {
      plState = PS_NORMAL;
    }

    return true;
  }
  else
  {
    PlaySound(SND_ENEMY_HIT);

    return false;
  }
}


/** Handle actor being hit by a player shot
 *
 * Unlike UpdateActorPlayerCollision(), this function doesn't perform collision
 * detection by itself. It expects to be invoked after we determined that an
 * actor was hit or took damage.
 *
 * It primarily defines what kind of effects (explosions, particles, debris) to
 * trigger when an actor is destroyed by the player.
 */
void pascal HandleActorShotCollision(int damage, word handle)
{
  register ActorState* state = gmActorStates + handle;
  register int i;

  if (!damage)
  {
    return;
  }

  switch (state->id)
  {
    case ACT_SUPER_FORCE_FIELD_L:
      // Play back an animation of an electrical arc. See Act_SuperForceField().
      if (!state->var1)
      {
        state->var1 = true;
      }
      break;

    case ACT_RED_BIRD:
      if (DamageActor(damage, handle))
      {
        state->deleted = true;
        GiveScore(100);
        SpawnParticles(state->x, state->y, 0, CLR_RED);
        SpawnEffect(ACT_EXPLOSION_FX_1, state->x, state->y, EM_NONE, 0);
      }
      break;

    case ACT_BOSS_EPISODE_2:
      if (DamageActor(damage, handle))
      {
        // Trigger death animation
        state->var5 = 1;
      }

      gmBossHealth = state->health;
      break;

    case ACT_BOSS_EPISODE_1:
      if (DamageActor(damage, handle))
      {
        // Trigger death animation
        state->var3 = 2;
      }

      gmBossHealth = state->health;
      break;

    case ACT_BOSS_EPISODE_3:
      if (DamageActor(damage, handle))
      {
        // Trigger death animation
        state->var3 = 2;
      }

      gmBossHealth = state->health;
      break;

    case ACT_BOSS_EPISODE_4:
      if (DamageActor(damage, handle))
      {
        // Trigger death animation
        state->var3 = 2;
      }

      gmBossHealth = state->health;
      break;

    case ACT_EYEBALL_THROWER_L:
    case ACT_EYEBALL_THROWER_R:
      if (DamageActor(damage, handle))
      {
        // clang-format off
        int DEBRIS_SPEC[] = { 5,
          0, -6, EM_FLY_UP, 0,
          0, -5, EM_FLY_LEFT, 1,
          0, -4, EM_FLY_RIGHT, 0,
          0, -3, EM_FLY_UPPER_LEFT, 1,
          0, -1, EM_FLY_UP, 0
        };
        // clang-format on

        PlaySound(SND_BIOLOGICAL_ENEMY_DESTROYED);
        state->deleted = true;
        SpawnDestructionEffects(handle, DEBRIS_SPEC, ACT_EYEBALL_PROJECTILE);
        SpawnParticles(state->x, state->y, 0, CLR_LIGHT_GREEN);
      }
      break;

    case ACT_MISSILE_BROKEN:
    case ACT_MISSILE_INTACT:
      if (DamageActor(damage, handle))
      {
        if (state->var3)
        {
          // Missile is intact, trigger launch
          state->var1 = 1;
        }
        else
        {
          // Missile is broken, trigger fall over animation
          //
          // [BUG] Because SpawnActorInSlot() never sets var3, this code path
          // here is taken for both types of missile. The consequence is that
          // shooting an intact missile will skip one frame of the launch
          // sequence in case it's shot from the left (i.e., projectile was
          // flying right).
          //
          // I'm not sure why the code here doesn't check state->id instead of
          // var3.
          if (retPlayerShotDirection == SD_LEFT)
          {
            state->var1 = 1;
          }
          else
          {
            state->var1 = 2;
          }
        }

        SpawnParticles(state->x + 5, state->y, 0, CLR_WHITE);
      }
      break;

    case ACT_ELECTRIC_REACTOR:
      if (DamageActor(damage, handle))
      {
        FLASH_SCREEN(SFC_YELLOW);
        SpawnPlayerShot(ACT_REACTOR_FIRE_L, state->x, state->y, SD_LEFT);
        SpawnPlayerShot(ACT_REACTOR_FIRE_R, state->x, state->y, SD_RIGHT);
        SpawnBurnEffect(
          ACT_WHITE_CIRCLE_FLASH_FX, state->id, state->x, state->y);

        // Make the sprite appear for a few more frames after the actor is
        // deleted
        SpawnEffect(ACT_ELECTRIC_REACTOR, state->x, state->y, EM_NONE, 0);

        // [NOTE] This spawns 24 effects in total. There can only be 18 effects
        // at max, and we've already used 2 effect slots for the burn effect
        // and the placeholder sprite. So only the first 16 loop iterations
        // actually have any effect. In case more effects are already present
        // at the time the reactor is destroyed, the number will be even lower.
        for (i = 0; i < 12; i++)
        {
          SpawnEffect(
            ACT_FLAME_FX,
            state->x + 1,
            state->y - 9 + i,
            i & 2 ? EM_FLY_LEFT : EM_FLY_RIGHT,
            i * 3);
          SpawnEffect(
            ACT_SMOKE_CLOUD_FX, state->x - 1, state->y - 9 + i, EM_NONE, i * 2);
        }

        PLAY_EXPLOSION_SOUND();

        // Switch to the alternate backdrop in case the "reactor destruction
        // event" is active for the current level. This is used in E1L5.
        if (mapHasReactorDestructionEvent)
        {
          bdAddressAdjust = 0x4000;
        }

        state->deleted = true;
      }
      break;

    case ACT_SLIME_CONTAINER:
      if (DamageActor(damage, handle))
      {
        // Trigger the "container breaking" animation. The container actor
        // stays active, it plays the animation and then spawns a slime blob.
        // See Act_SlimeContainer.
        state->var1 = 1;
        state->frame = 2;
        PlaySound(SND_GLASS_BREAKING);
        state->drawStyle = DS_WHITEFLASH;
        SpawnParticles(state->x + 1, state->y, 0, CLR_WHITE);
      }
      break;

    case ACT_HOVERBOT:
    case ACT_BOSS_EPISODE_4_SHOT:
      if (DamageActor(damage, handle))
      {
        SpawnBurnEffect(ACT_FLAME_FX, state->id, state->x, state->y);

        if (state->id == ACT_HOVERBOT)
        {
          SpawnEffect(
            ACT_HOVERBOT_DEBRIS_1, state->x, state->y - 2, EM_FLY_UP, 0);
          SpawnEffect(
            ACT_HOVERBOT_DEBRIS_2, state->x, state->y, EM_FLY_DOWN, 0);
        }

        SpawnParticles(state->x + 1, state->y, 0, CLR_LIGHT_GREY);

        PLAY_EXPLOSION_SOUND();
        state->deleted = true;
      }
      break;

    case ACT_BLUE_GUARD_R:
      // If the guard is typing on a terminal, interrupt the typing and
      // turn to face the player
      if (state->var5 > 1)
      {
        state->var5 = 1;

        if (state->x > plPosX)
        {
          state->var1 = 1;
        }
        else
        {
          state->var1 = 0;
        }
      }

      if (DamageActor(damage, handle))
      {
        SpawnParticles(state->x + 1, state->y, 0, CLR_LIGHT_BLUE);
        PLAY_EXPLOSION_SOUND();
        SpawnBurnEffect(ACT_FLAME_FX, state->id, state->x, state->y);
        state->deleted = true;
      }
      break;

    case ACT_BIG_GREEN_CAT_L:
    case ACT_BIG_GREEN_CAT_R:
    case ACT_WATCHBOT:
    case ACT_ROCKET_LAUNCHER_TURRET:
    case ACT_SLIME_BLOB:
    case ACT_SLIME_BLOB_2:
    case ACT_CEILING_SUCKER:
    case ACT_UGLY_GREEN_BIRD:
    case ACT_GREEN_CREATURE_L:
    case ACT_GREEN_CREATURE_R:
      if (DamageActor(damage, handle))
      {
        SpawnBurnEffect(ACT_FLAME_FX, state->id, state->x, state->y);
        SpawnParticles(state->x + 1, state->y, 0, RandomNumber() & 15);

        // clang-format off
        if (
          state->id == ACT_BIG_GREEN_CAT_L ||
          state->id == ACT_BIG_GREEN_CAT_R ||
          state->id == ACT_GREEN_CREATURE_L ||
          state->id == ACT_GREEN_CREATURE_R ||
          state->id == ACT_CEILING_SUCKER ||
          state->id == ACT_SLIME_BLOB_2 ||
          state->id == ACT_SLIME_BLOB ||
          state->id == ACT_UGLY_GREEN_BIRD)
        {
          int DEBRIS_SPEC[] = { 6,
            1, 2, EM_FLY_UP, 0,
            0, 0, EM_FLY_UPPER_RIGHT, 1,
            -1, 1, EM_FLY_UPPER_LEFT, 2,
            1, -1, EM_FLY_DOWN, 3,
            -1, 2, EM_FLY_UPPER_RIGHT, 4,
            1, 2, EM_FLY_UPPER_LEFT, 5
          };

          SpawnDestructionEffects(
            handle, DEBRIS_SPEC, ACT_BIOLOGICAL_ENEMY_DEBRIS);

          if (
            state->id == ACT_GREEN_CREATURE_L ||
            state->id == ACT_GREEN_CREATURE_R)
          {
            state->y -= 2;
            state->x -= 2;
            SpawnDestructionEffects(
              handle, DEBRIS_SPEC, ACT_BIOLOGICAL_ENEMY_DEBRIS);

            state->x += 2;
            state->y -= 2;
            SpawnDestructionEffects(
              handle, DEBRIS_SPEC, ACT_BIOLOGICAL_ENEMY_DEBRIS);
          }

          PlaySound(SND_BIOLOGICAL_ENEMY_DESTROYED);
        }
        else
        {
          PLAY_EXPLOSION_SOUND();
        }
        // clang-format on

        state->deleted = true;
      }
      break;

    case ACT_SKELETON:
    case ACT_RIGELATIN_SOLDIER:
      if (DamageActor(damage, handle))
      {
        PLAY_EXPLOSION_SOUND();

        if (state->id == ACT_SKELETON)
        {
          SpawnBurnEffect(ACT_FLAME_FX, state->id, state->x, state->y);
        }
        else
        {
          SpawnBurnEffect(ACT_EXPLOSION_FX_1, state->id, state->x, state->y);
        }

        SpawnParticles(state->x + 1, state->y, 0, RandomNumber() & 15);
        GiveScore(100);
        state->deleted = true;
      }
      break;

    case ACT_SPIDER:
      // Spider can't be damaged if attached to the player
      // [NOTE] This is redundant, since the spider's health is set
      // to 0 when attaching to the player, and that excludes it from
      // shot collision detection.
      if (
        plAttachedSpider1 == handle || plAttachedSpider2 == handle ||
        plAttachedSpider3 == handle)
      {
        break;
      }

      if (DamageActor(damage, handle))
      {
        SpawnEffect(ACT_EXPLOSION_FX_1, state->x - 1, state->y + 1, EM_NONE, 0);
        GiveScore(100);
        state->deleted = true;
      }
      break;

    case ACT_AGGRESSIVE_PRISONER:
      // Only allow being damaged while grabbing
      if (state->var1 != 2)
      {
        // [BUG] This should set health to 0 so that the actor doesn't
        // participate in collision detection anymore.  Because that's not done
        // here, the actor still causes player shots to stop when hitting it
        // during its death animation.

        PlaySound(SND_BIOLOGICAL_ENEMY_DESTROYED);

        switch (retPlayerShotDirection)
        {
          case SD_LEFT:
            SpawnEffect(
              ACT_PRISONER_HAND_DEBRIS,
              state->x,
              state->y,
              EM_FLY_UPPER_LEFT,
              0);
            break;

          case SD_RIGHT:
            SpawnEffect(
              ACT_PRISONER_HAND_DEBRIS,
              state->x,
              state->y,
              EM_FLY_UPPER_RIGHT,
              0);
            break;
        }

        state->var1 = 2;
        state->frame = 5;

        SpawnParticles(state->x + 3, state->y, 0, CLR_RED);
        GiveScore(500);
      }
      break;

    case ACT_LASER_TURRET:
      if (state->var1 == 0) // not currently spinning
      {
        // [NOTE] I'm not sure why there are two variables to track the player's
        // current weapon. This is the only place where this alternate variable
        // is used, everything else uses plWeapon.
        // The variable is set in HUD_DrawWeapon().
        if (plWeapon_hud != WPN_REGULAR || plState == PS_USING_SHIP)
        {
          switch (retPlayerShotDirection)
          {
            case SD_LEFT:
              SpawnEffect(
                ACT_LASER_TURRET, state->x, state->y, EM_FLY_UPPER_LEFT, 0);
              break;

            case SD_RIGHT:
              SpawnEffect(
                ACT_LASER_TURRET, state->x, state->y, EM_FLY_UPPER_RIGHT, 0);
              break;

            default:
              SpawnEffect(ACT_LASER_TURRET, state->x, state->y, EM_FLY_UP, 0);
              break;
          }

          GiveScore(499); // 1 point is already given by the code below
          state->deleted = true;
          gmTurretsDestroyed++;
        }

        SpawnEffect(ACT_FLAME_FX, state->x - 1, state->y + 2, EM_RISE_UP, 0);
        PLAY_EXPLOSION_SOUND();
        state->var1 = 40; // Make the turret spin (see Act_LaserTurret())
        GiveScore(1);
      }
      break;

    case ACT_BOUNCING_SPIKE_BALL:
      // Make it fly left/right when hit on either side. See Act_SpikeBall.
      if (retPlayerShotDirection == SD_LEFT)
      {
        state->var1 = 1;
      }
      else if (retPlayerShotDirection == SD_RIGHT)
      {
        state->var1 = 2;
      }

      if (DamageActor(damage, handle))
      {
        SpawnParticles(state->x + 1, state->y, 0, CLR_WHITE);
        SpawnEffect(ACT_EXPLOSION_FX_1, state->x - 1, state->y + 1, EM_NONE, 0);
        state->deleted = true;
      }
      break;

    case ACT_SMALL_FLYING_SHIP_1:
    case ACT_SMALL_FLYING_SHIP_2:
    case ACT_SMALL_FLYING_SHIP_3:
      SpawnParticles(state->x, state->y, 0, RandomNumber() & 15);
      state->deleted = true;
      PLAY_EXPLOSION_SOUND();
      GiveScore(100);
      break;

    case ACT_CAMERA_ON_CEILING:
    case ACT_CAMERA_ON_FLOOR:
      SpawnParticles(state->x, state->y, 0, RandomNumber() & 15);
      SpawnEffect(
        ACT_SCORE_NUMBER_FX_100, state->x, state->y, EM_SCORE_NUMBER, 0);
      state->deleted = true;
      gmCamerasDestroyed++;
      PLAY_EXPLOSION_SOUND();
      GiveScore(100);
      break;

    case ACT_FLAME_THROWER_BOT_R:
    case ACT_FLAME_THROWER_BOT_L:
    case ACT_ENEMY_ROCKET_LEFT:
    case ACT_ENEMY_ROCKET_UP:
    case ACT_ENEMY_ROCKET_RIGHT:
    case ACT_WATCHBOT_CONTAINER_CARRIER:
    case ACT_BOMBER_PLANE:
    case ACT_SNAKE:
    case ACT_WALL_WALKER:
    case ACT_MESSENGER_DRONE_BODY:
    case ACT_HOVERBOT_GENERATOR:
    case ACT_HOVERING_LASER_TURRET:
    case ACT_FLOATING_EXIT_SIGN_R:
    case ACT_RADAR_DISH:
    case ACT_SPECIAL_HINT_GLOBE:
    case ACT_UNICYCLE_BOT:
    case ACT_FLOATING_EXIT_SIGN_L:
    case ACT_FLOATING_ARROW:
      if (DamageActor(damage, handle))
      {
        // clang-format off
        int DEBRIS_SPEC[] = { 3,
           0,  0, EM_NONE, 0,
          -1, -2, EM_NONE, 2,
           1, -3, EM_NONE, 4,
        };
        // clang-format on

        SpawnDestructionEffects(handle, DEBRIS_SPEC, ACT_EXPLOSION_FX_1);

        state->deleted = true;

        SpawnParticles(state->x, state->y, 0, RandomNumber() & 15);
        SpawnParticles(state->x - 1, state->y - 1, -1, RandomNumber() & 15);
        SpawnParticles(state->x + 1, state->y - 2, 1, RandomNumber() & 15);

        if (state->id == ACT_RADAR_DISH)
        {
          gmRadarDishesLeft--;
          SpawnEffect(
            ACT_SCORE_NUMBER_FX_2000, state->x, state->y, EM_SCORE_NUMBER, 0);

          // For some reason, the actor itself has a score of 500 (given by
          // DamageActor()), but the actual score value is 2000. So 1500 extra
          // points are given here.
          GiveScore(1500);
        }

        if (
          state->id == ACT_FLOATING_EXIT_SIGN_R ||
          state->id == ACT_FLOATING_EXIT_SIGN_L)
        {
          SpawnEffect(
            ACT_SCORE_NUMBER_FX_10000, state->x, state->y, EM_SCORE_NUMBER, 0);
          GiveScore(10000);
        }

        if (state->id == ACT_FLOATING_ARROW)
        {
          SpawnEffect(
            ACT_SCORE_NUMBER_FX_500, state->x, state->y, EM_SCORE_NUMBER, 0);
          GiveScore(500);
        }
      }
      break;

    case ACT_NUCLEAR_WASTE_CAN_EMPTY:
    case ACT_WHITE_BOX:
    case ACT_GREEN_BOX:
    case ACT_RED_BOX:
    case ACT_BLUE_BOX:
      if (DamageActor(damage, handle) && !state->var1)
      {
        // Trigger the "spawn item" sequence in Act_ItemBox
        state->var1 = 1;

        PLAY_EXPLOSION_SOUND();

        SpawnParticles(state->x + 1, state->y, 0, CLR_DARK_RED);
        SpawnParticles(state->x + 1, state->y, 0, CLR_WHITE);
        SpawnParticles(state->x + 1, state->y, 0, CLR_LIGHT_BLUE);
      }
      break;

    case ACT_TURKEY:
      PlaySound(SND_BIOLOGICAL_ENEMY_DESTROYED);

      if (state->var2 != 2) // should always be true
      {
        SpawnEffect(ACT_SMOKE_CLOUD_FX, state->x, state->y, EM_NONE, 0);
      }

      state->health = 0; // make invincible

      // This turns a walking turkey into a cooked turkey. See Act_Turkey.
      state->var2 = 2;
      break;

    case ACT_SODA_CAN:
    case ACT_SODA_6_PACK:
      if (!state->var3)
      {
        // Trigger either the "soda can rocket" or make a six pack explode.
        // See Act_ItemBox.
        state->var3 = 1;
      }
      break;

    case ACT_MINI_NUKE:
      SpawnEffect(ACT_NUCLEAR_EXPLOSION, state->x, state->y, EM_NONE, 0);

      for (i = 4; i < 20; i += 4)
      {
        SpawnEffect(
          ACT_NUCLEAR_EXPLOSION, state->x - i, state->y, EM_FLY_DOWN, i >> 1);
        SpawnEffect(
          ACT_NUCLEAR_EXPLOSION, state->x + i, state->y, EM_FLY_DOWN, i >> 1);
      }

      PLAY_EXPLOSION_SOUND();
      state->deleted = true;
      break;

    case ACT_RED_BOX_BOMB:
      {
        bool spawnFailedLeft = false;
        bool spawnFailedRight = false;
        i = 0;

        PLAY_EXPLOSION_SOUND();

        gmBombBoxesLeft--;

        SpawnParticles(state->x + 1, state->y, 0, CLR_WHITE);

        for (i = 0; i < 12; i += 2)
        {
          if (!spawnFailedLeft)
          {
            spawnFailedLeft += SpawnEffect(
              ACT_FIRE_BOMB_FIRE, state->x - 2 - i, state->y, EM_NONE, i);
          }

          if (!spawnFailedRight)
          {
            spawnFailedRight += SpawnEffect(
              ACT_FIRE_BOMB_FIRE, state->x + i + 2, state->y, EM_NONE, i);
          }
        }

        state->deleted = true;
      }
      break;

    case ACT_BONUS_GLOBE_SHELL:
      SpawnEffect(
        ACT_BONUS_GLOBE_DEBRIS_1, state->x, state->y, EM_FLY_UPPER_LEFT, 0);
      SpawnEffect(
        ACT_BONUS_GLOBE_DEBRIS_2,
        state->x + 2,
        state->y,
        EM_FLY_UPPER_RIGHT,
        0);
      SpawnEffect(state->var1, state->x, state->y, EM_FLY_UP, 0);

      state->drawStyle = DS_WHITEFLASH;

      GiveScore(100);
      SpawnEffect(
        ACT_SCORE_NUMBER_FX_100, state->x, state->y, EM_SCORE_NUMBER, 0);
      PlaySound(SND_GLASS_BREAKING);
      SpawnParticles(state->x + 1, state->y, 0, CLR_WHITE);

      state->deleted = true;
      gmOrbsLeft--;
      break;
  }
}


/** Utility function for moving actors around while respecting world collision
 *
 * This function checks if the given actor collides with the world (i.e., a
 * wall, floor, or ceiling) in the given direction. It must be called _after_
 * modifying the actor's position to move in the intended direction. If there's
 * a collision, the actor's position is adjusted in the _opposite_ direction
 * to undo the move. So the intended usage looks like this:
 *
 *   // Move actor right
 *   state->x += 1;
 *
 *   // Check collision and undo move if necessary
 *   ApplyWorldCollision(handle, MD_RIGHT);
 *
 * The function will return a non-zero value if there was a collision, 0
 * otherwise. This can be used to make an actor turn around after reaching
 * a wall, for example.
 *
 * TODO: Document the allowStairStepping flag
 */
int pascal ApplyWorldCollision(word handle, word direction)
{
  register ActorState* actor = gmActorStates + handle;

  if (direction == MD_UP || direction == MD_DOWN)
  {
    register int result = CheckWorldCollision(
      direction, actor->id, actor->frame, actor->x, actor->y);

    if (result)
    {
      if (direction == MD_UP)
      {
        actor->y++;
      }
      else
      {
        actor->y--;
      }
    }

    return result;
  }

  if (actor->gravityAffected && actor->allowStairStepping)
  {
    actor->y--;
  }

  if (direction == MD_LEFT)
  {
    register ibool canMove = !CheckWorldCollision(
      MD_LEFT, actor->id, actor->frame, actor->x, actor->y);

    if (!canMove)
    {
      actor->x++;
    }
    else
    {
      if (CheckWorldCollision(
            MD_DOWN, actor->id, actor->frame, actor->x - 2, actor->y + 1))
      {
        canMove = true;
      }
      else if (!actor->allowStairStepping)
      {
        canMove = false;
        actor->x++;
      }
    }

    if (actor->gravityAffected && actor->allowStairStepping)
    {
      actor->y++;
    }

    return !canMove;
  }
  else // moving to the right
  {
    register ibool canMove = !CheckWorldCollision(
      MD_RIGHT, actor->id, actor->frame, actor->x, actor->y);

    if (!canMove)
    {
      actor->x--;
    }
    else
    {
      if (CheckWorldCollision(
            MD_DOWN, actor->id, actor->frame, actor->x + 2, actor->y + 1))
      {
        canMove = true;
      }
      else if (!actor->allowStairStepping)
      {
        canMove = false;
        actor->x--;
      }
    }

    if (actor->gravityAffected && actor->allowStairStepping)
    {
      actor->y++;
    }

    return !canMove;
  }
}


/** Check if center-to-center distance between actor & player is below value */
bool pascal PlayerInRange(word handle, word distance)
{
  register ActorState* actor = gmActorStates + handle;
  register word offset;
  word width;
  word actorCenterX;
  word playerOffsetToCenter = 1;

  offset = gfxActorInfoData[actor->id] + (actor->frame << 3);
  width = AINFO_WIDTH(offset);
  actorCenterX = actor->x + width / 2;

  // This is to account for the player's weapon, which protrudes to the left
  // if the player is facing left. That part of the sprite isn't considered
  // part of the player, so we offset by 1 to get the distance to the center
  // of the player's body.
  if (plActorId == ACT_DUKE_L)
  {
    playerOffsetToCenter += 1;
  }

  if (DN2_abs(actorCenterX - (plPosX + playerOffsetToCenter)) <= distance)
  {
    return true;
  }
  else
  {
    return false;
  }
}


/** Spawn a new actor into the game world
 *
 * Creates an actor of the given type at the given location.
 * Tries to reuse the state slot of a previously deleted actor if possible,
 * otherwise the actor is added to the end of the list.
 */
void pascal SpawnActor(word id, word x, word y)
{
  int i;

  // First, see if there's a free slot (actor that was deleted), and use it if
  // we find one
  for (i = 0; i < gmNumActors; i++)
  {
    ActorState* actor = gmActorStates + i;

    if (actor->deleted)
    {
      SpawnActorInSlot(i, id, x, y);
      return;
    }
  }

  // Otherwise, place the actor at the end of the list if the maximum number of
  // actors hasn't been reached yet. If it has, fail silently.
  if (gmNumActors < MAX_NUM_ACTORS)
  {
    SpawnActorInSlot(gmNumActors, id, x, y);
    gmNumActors++;
  }
}


/** Updates and draws all actors
 *
 * This is the main entry point into the actor system.  It goes through all
 * actors that are currently in the game world, determines which ones should be
 * active, calls their update functions, draws their sprites, applies gravity,
 * checks collision against the player and their shots, etc.
 *
 * Also handles updating the top-row HUD message, and draws the radar in the
 * HUD.
 */
void UpdateAndDrawActors(void)
{
  register word handle;
  register word numActors = gmNumActors;
  ActorState* actor;
  word savedDrawStyle;

  for (handle = 0; handle < numActors; handle++)
  {
    actor = gmActorStates + handle;

    // Save the current draw style so we can restore it later
    savedDrawStyle = actor->drawStyle;

    // Skip deleted actors
    if (actor->deleted)
    {
      continue;
    }

    //
    // Active state handling
    //
    if (IsActorOnScreen(handle))
    {
      // Actors which have the 'remain active' flag set are given the
      // 'always update' flag when they appear on screen
      if (actor->remainActive)
      {
        actor->alwaysUpdate = true;
      }
    }
    else if (!actor->alwaysUpdate)
    {
      // Skip actors that aren't on screen, unless they have the 'always update'
      // flag set
      continue;
    }

    //
    // Physics - gravity and conveyor belt movement
    //
    if (actor->gravityAffected)
    {
      // gravityState can take on the following values:
      //   0 - actor is on ground/not falling
      //   1 - actor is in the air, but not falling yet
      //   2 - actor is falling with a speed of 1
      //   3 - same as 2
      //   4 - actor is falling with a speed of 2 (max falling speed)

      // If the actor is currently stuck in the ground, move it up by one unit
      if (CheckWorldCollision(
            MD_DOWN, actor->id, actor->frame, actor->x, actor->y))
      {
        actor->y--;
        actor->gravityState = 0;
      }

      // Is the actor currently in the air?
      if (!CheckWorldCollision(
            MD_DOWN, actor->id, actor->frame, actor->x, actor->y + 1))
      {
        // Apply acceleration
        if (actor->gravityState < 4)
        {
          actor->gravityState++;
        }

        if (actor->gravityState > 1 && actor->gravityState < 5)
        {
          actor->y++;
        }

        // state of 4 means fall 2 units per frame, so move one additional unit
        if (actor->gravityState == 4)
        {
          if (!CheckWorldCollision(
                MD_DOWN, actor->id, actor->frame, actor->x, actor->y + 1))
          {
            actor->y++;
          }
          else
          {
            // Actor has reached the ground, stop falling
            actor->gravityState = 0;
          }
        }
      }
      else // not in the air
      {
        actor->gravityState = 0;

        // Conveyor belt movement
        if (
          retConveyorBeltCheckResult == 1 &&
          !CheckWorldCollision(
            MD_LEFT, actor->id, actor->frame, actor->x - 1, actor->y))
        {
          actor->x--;
        }
        else if (
          retConveyorBeltCheckResult == 2 &&
          !CheckWorldCollision(
            MD_RIGHT, actor->id, actor->frame, actor->x + 1, actor->y))
        {
          actor->x++;
        }
      }
    }


    //
    // Update, collision testing and drawing
    //

    // Invoke actor-specific update logic
    actor->updateFunc(handle);

    // Delete vertically out-of-bounds actors, unless it's the player
    if (
      actor->id != ACT_DUKE_L && actor->id != ACT_DUKE_R &&
      actor->y > mapBottom)
    {
      actor->deleted = true;
      continue;
    }

    // Invisible actors aren't drawn and don't participate in collision
    // detection
    if (actor->drawStyle != DS_INVISIBLE)
    {
      // Test for shot collision and handle as applicable. Actors with health
      // of 0 are invincible.
      if (actor->health > 0)
      {
        HandleActorShotCollision(TestShotCollision(handle), handle);
      }

      if (!actor->deleted) // If the actor wasn't killed by a shot
      {
        UpdateActorPlayerCollision(handle);

        if (IsActorOnScreen(handle))
        {
          DrawActor(
            actor->id, actor->frame, actor->x, actor->y, actor->drawStyle);
        }

        HUD_ShowOnRadar(actor->x, actor->y);
      }
    }

    // Restore previous display style, in case it was changed by the update
    // function
    actor->drawStyle = savedDrawStyle;
  }
}

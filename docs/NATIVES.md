# API de scripts (natives) do jogo

Fonte: relatório da demo real (119/119 scripts decodificados por completo). São **115 funções nativas** que a engine precisa oferecer à VM; a coluna de chamadas é a contagem de pontos de chamada no código de todos os scripts (indica prioridade).

## Ações de personagem (A_*) (11)

| Função | Chamadas |
|---|---:|
| `A_Wait` | 38 |
| `A_PlayAnim` | 36 |
| `A_FloatingSpeechText` | 21 |
| `A_MoveToPosition` | 20 |
| `A_Attack` | 18 |
| `A_Wander` | 10 |
| `A_MoveToEntity` | 9 |
| `A_SendMessage` | 3 |
| `A_JustAttack` | 2 |
| `A_Kill` | 1 |
| `A_ReloadWeapon` | 1 |

## Diálogo e texto (11)

| Função | Chamadas |
|---|---:|
| `RegisterReply` | 1680 |
| `SetNodeTextFunction` | 1245 |
| `RegisterNodeText` | 642 |
| `SetCurrentNodeName` | 508 |
| `PCSpeaker` | 291 |
| `MessageGetArg` | 284 |
| `FloatingSpeechText` | 225 |
| `AddEntry` | 165 |
| `FloatingDamageText` | 156 |
| `DisplayStringRef` | 89 |
| `CloseTask` | 49 |

## Biblioteca de ponto flutuante do Pawn (8)

| Função | Chamadas |
|---|---:|
| `float` | 334 |
| `floatdiv` | 204 |
| `floatcmp` | 141 |
| `floatadd` | 99 |
| `floatsub` | 96 |
| `floatmul` | 84 |
| `floatround` | 79 |
| `floatfract` | 78 |

## Leitura de estado (Get*, Is*, Has*, Find*, Distance...) (46)

| Função | Chamadas |
|---|---:|
| `Myself` | 7310 |
| `GetGender` | 970 |
| `GetGlobal` | 865 |
| `GetRace` | 583 |
| `GetSquadID` | 536 |
| `GetTeam` | 475 |
| `GetTimeStamp` | 332 |
| `GetCurrentHP` | 299 |
| `Protagonist` | 196 |
| `GetTeamRank` | 194 |
| `IsDead` | 177 |
| `GetPrimaryAttribute` | 175 |
| `GetTotalSkill` | 175 |
| `GetPosition` | 163 |
| `GetSquadRank` | 144 |
| `GetObject` | 101 |
| `GetTeamAttitude` | 100 |
| `GetFacing` | 100 |
| `Hour` | 98 |
| `FindObjects` | 97 |
| `GetMaxHP` | 80 |
| `Distance` | 67 |
| `GetDirectionToEntity` | 50 |
| `GetLOS` | 49 |
| `GetSquadAttitude` | 49 |
| `IsEntityInArea` | 47 |
| `GetType` | 46 |
| `IsIdle` | 44 |
| `FindEntity` | 38 |
| `IsInParty` | 16 |
| `IsLocked` | 14 |
| `GetWayPointCount` | 8 |
| `GetWayPointPosition` | 7 |
| `IsItemEquipped` | 7 |
| `GetMapID` | 5 |
| `HasItem` | 5 |
| `GetNavPointCount` | 4 |
| `HasDied` | 4 |
| `GetVEGID` | 3 |
| `GetNavPointPosition` | 3 |
| `IsUnderFOW` | 2 |
| `GetNearestPath` | 2 |
| `IsInDialogue` | 2 |
| `Minute` | 2 |
| `GetLOSPoint` | 1 |
| `FindTeamMembers` | 1 |

## Alteração de estado (Set*, Add*, Remove*, Create*, Destroy*...) (23)

| Função | Chamadas |
|---|---:|
| `AttachSound` | 1949 |
| `SetGlobal` | 375 |
| `SetSquadAttitude` | 362 |
| `RemoveSound` | 292 |
| `SetValue` | 156 |
| `AddHP` | 156 |
| `SetSquadRank` | 145 |
| `AddToSquad` | 144 |
| `ClearActions` | 85 |
| `SetTeamAttitude` | 64 |
| `DestroyEntity` | 49 |
| `CreateSquad` | 48 |
| `CreateTeam` | 48 |
| `AddToTeam` | 48 |
| `SetFacingTarget` | 26 |
| `AttachVEG` | 5 |
| `DeactivateEntity` | 3 |
| `RemoveVEG` | 3 |
| `SetFacing` | 3 |
| `EquipItem` | 2 |
| `ActivateEntity` | 2 |
| `SetModel` | 1 |
| `SetTeamRank` | 1 |

## Outras (16)

| Função | Chamadas |
|---|---:|
| `RandomNumber` | 667 |
| `SecondsPassed` | 97 |
| `SendMessage` | 87 |
| `MessageArgCount` | 80 |
| `UnlockDoor` | 78 |
| `OpenTask` | 54 |
| `StartStore` | 42 |
| `MapEffectSetActive` | 10 |
| `GiveNewItem` | 10 |
| `SendMessageToTeam` | 6 |
| `PointDistance` | 5 |
| `PartyHasItem` | 5 |
| `MatchEntity` | 3 |
| `StartDialogue` | 2 |
| `MapEffectResetElapsedTime` | 1 |
| `LockDoor` | 1 |

## Como implementar

1. Registre cada função em `f3vb::amx::Machine::register_native(nome, callback)`; `params[0]` é o nº de bytes de argumentos e `params[1..]` os argumentos (inteiros de 32 bits; floats do Pawn são o mesmo padrão de bits).
2. Comece pelas mais chamadas e pelo grupo de diálogo; as de ponto flutuante são triviais.
3. Strings chegam como endereços na memória do script (`read_string`).

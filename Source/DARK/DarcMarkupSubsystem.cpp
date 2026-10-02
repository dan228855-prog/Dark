// DarcMarkupSubsystem.cpp
#include "DarcMarkupSubsystem.h"
#include "DarcAssetSettings.h"
#include "DarcGameplayLibrary.h"
#include "DarcHintComponent.h"
#include "InteractableDoor.h"
#include "CarryableItem.h"
#include "DarcFuseBox.h"
#include "DarcFuseItem.h"
#include "DarcPowerLamp.h"
#include "DarcPowerConsumerComponent.h"
#include "DarcPowerInlet.h"
#include "DarcGenerator.h"
#include "DarcHeavyObject.h"
#include "DarcTerminal.h"
#include "DarcCardReader.h"
#include "DarcItemSlot.h"
#include "DarcDataTransferStation.h"
#include "DarcNpc.h"
#include "DarcRareEventAnchor.h"
#include "DarcRoomVolume.h"
#include "DarcSliceItems.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogDarcMarkup, Log, All);

namespace DarcMarkup
{
	const FString Prefix = TEXT("DARC.");

	FText Txt(const FString& Key) { return UDarcGameplayLibrary::UIText(*Key); }

	TArray<FText> Texts(const TArray<FString>& Keys)
	{
		TArray<FText> Out;
		for (const FString& Key : Keys)
		{
			Out.Add(Txt(Key));
		}
		return Out;
	}

	/** Реплики NPC из списка ключей: длительность — по длине текста. */
	TArray<FDarcNpcLine> Lines(const TArray<FString>& Keys, float FirstDelay = 0.3f)
	{
		TArray<FDarcNpcLine> Out;
		for (const FString& Key : Keys)
		{
			FDarcNpcLine Line;
			Line.Text = Txt(Key);
			Line.DelayBefore = Out.Num() == 0 ? FirstDelay : 0.8f;
			Line.Duration = FMath::Clamp(Line.Text.ToString().Len() * 0.06f, 2.f, 6.f);
			Out.Add(Line);
		}
		return Out;
	}

	/** Класс предмета по короткому имени: Keycard, Drive, Module, Fuse, Item. */
	UClass* ItemClass(const FString& Name)
	{
		if (Name.Equals(TEXT("Keycard"), ESearchCase::IgnoreCase) || Name.Equals(TEXT("Card"), ESearchCase::IgnoreCase))
		{
			return ADarcKeycard::StaticClass();
		}
		if (Name.Equals(TEXT("Drive"), ESearchCase::IgnoreCase))
		{
			return ADarcDataDrive::StaticClass();
		}
		if (Name.Equals(TEXT("Module"), ESearchCase::IgnoreCase) || Name.Equals(TEXT("InterfaceModule"), ESearchCase::IgnoreCase))
		{
			return ADarcInterfaceModule::StaticClass();
		}
		if (Name.Equals(TEXT("Fuse"), ESearchCase::IgnoreCase))
		{
			return ADarcFuseItem::StaticClass();
		}
		return Name.IsEmpty() ? nullptr : ACarryableItem::StaticClass();
	}

	FDarcVisualSpec CopyOf(AActor* Source)
	{
		FDarcVisualSpec Spec;
		Spec.CopyFrom = Source;
		return Spec;
	}

	/** Порядок создания: сначала то, на что ссылаются другие (предметы, двери, точки). */
	int32 Priority(FName Type)
	{
		static const TArray<FName> Order = {
			TEXT("Point"), TEXT("Room"), TEXT("Item"), TEXT("Keycard"), TEXT("Drive"), TEXT("Module"), TEXT("Fuse"),
			TEXT("Door"), TEXT("Lamp"), TEXT("FuseBox"), TEXT("Generator"), TEXT("Heavy"), TEXT("Inlet"),
			TEXT("Slot"), TEXT("Terminal"), TEXT("Keypad"), TEXT("CardReader"), TEXT("Station"), TEXT("Npc"), TEXT("Anchor") };
		const int32 Index = Order.IndexOfByPredicate([Type](const FName& N) { return N.IsEqual(Type, ENameCase::IgnoreCase); });
		return Index == INDEX_NONE ? Order.Num() : Index;
	}

	/** Типы без своего игрового объекта (исходный актор остаётся надписью или скрывается). */
	bool IsLocalOnly(FName Type) { return Type.IsEqual(TEXT("CodeSign"), ENameCase::IgnoreCase); }
	bool IsHiddenMarker(FName Type) { return Type.IsEqual(TEXT("Room"), ENameCase::IgnoreCase) || Type.IsEqual(TEXT("Point"), ENameCase::IgnoreCase); }
}

// ---------------------------------------------------------------------------
// FDarcMarkup
// ---------------------------------------------------------------------------

FString FDarcMarkup::Get(const TCHAR* Key, const FString& Default) const
{
	const FString* Found = Params.Find(FString(Key).ToLower());
	return Found ? *Found : Default;
}

FName FDarcMarkup::GetName(const TCHAR* Key, FName Default) const
{
	const FString Value = Get(Key);
	return Value.IsEmpty() ? Default : FName(*Value);
}

float FDarcMarkup::GetFloat(const TCHAR* Key, float Default) const
{
	const FString Value = Get(Key);
	return Value.IsNumeric() ? FCString::Atof(*Value) : Default;
}

TArray<FString> FDarcMarkup::GetList(const TCHAR* Key) const
{
	TArray<FString> Out;
	Get(Key).ParseIntoArray(Out, TEXT(","), true);
	for (FString& Item : Out)
	{
		Item.TrimStartAndEndInline();
	}
	return Out;
}

// ---------------------------------------------------------------------------
// Разбор
// ---------------------------------------------------------------------------

bool UDarcMarkupSubsystem::ParseMarkup(const AActor* Actor, FDarcMarkup& Out)
{
	if (!Actor)
	{
		return false;
	}
	bool bFound = false;
	for (const FName& Tag : Actor->Tags)
	{
		const FString Text = Tag.ToString().TrimStartAndEnd();
		if (Text.StartsWith(DarcMarkup::Prefix, ESearchCase::IgnoreCase))
		{
			TArray<FString> Parts;
			Text.RightChop(DarcMarkup::Prefix.Len()).ParseIntoArray(Parts, TEXT("."), true);
			if (Parts.Num() > 0)
			{
				bFound = true;
				Out.Type = FName(*Parts[0]);
				for (int32 i = 1; i < Parts.Num(); ++i)
				{
					Out.Flags.Add(Parts[i].ToLower());
				}
			}
		}
		else
		{
			FString Key, Value;
			if (Text.Split(TEXT("="), &Key, &Value))
			{
				Out.Params.Add(Key.TrimStartAndEnd().ToLower(), Value.TrimStartAndEnd());
			}
		}
	}
	Out.Source = const_cast<AActor*>(Actor);
	return bFound;
}

bool UDarcMarkupSubsystem::HasMarkup(const UWorld* World)
{
	if (!World)
	{
		return false;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		for (const FName& Tag : It->Tags)
		{
			if (Tag.ToString().StartsWith(DarcMarkup::Prefix, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
	}
	return false;
}

// ---------------------------------------------------------------------------
// Жизненный цикл
// ---------------------------------------------------------------------------

bool UDarcMarkupSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UDarcMarkupSubsystem::ApplyLocalMarkup()
{
	// У каждой машины (сервер и клиенты): карта у всех одна и та же.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		FDarcMarkup Markup;
		if (!ParseMarkup(*It, Markup))
		{
			continue;
		}
		if (DarcMarkup::IsHiddenMarker(Markup.Type))
		{
			It->SetActorHiddenInGame(true);
			It->SetActorEnableCollision(false);
		}
		else if (DarcMarkup::IsLocalOnly(Markup.Type))
		{
			// Табличка с кодом: цифры поверх модели, у каждого игрока свои (код одинаковый —
			// из реплицируемого сида выезда). Ставится на переднюю грань (+X актора).
			FVector Origin, Extent;
			It->GetActorBounds(true, Origin, Extent);
			UDarcCodeTextComponent* CodeText = NewObject<UDarcCodeTextComponent>(*It);
			CodeText->CodeKey = Markup.GetName(TEXT("Code"), TEXT("ServerDoorCode"));
			CodeText->SetWorldSize(Markup.GetFloat(TEXT("Size"), 20.f));
			CodeText->SetTextRenderColor(FColor(200, 40, 30));
			CodeText->SetMobility(EComponentMobility::Movable);
			CodeText->SetUsingAbsoluteScale(true);
			CodeText->RegisterComponent();
			CodeText->SetWorldLocationAndRotation(
				Origin + It->GetActorForwardVector() * (FVector::DotProduct(Extent, It->GetActorForwardVector().GetAbs()) + 1.f),
				It->GetActorRotation());
			It->AddInstanceComponent(CodeText);
		}
	}
}

void UDarcMarkupSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	ApplyLocalMarkup();
	if (InWorld.GetNetMode() != NM_Client)
	{
		ConvertMarkup();
	}
}

AActor* UDarcMarkupSubsystem::FindById(FName Id) const
{
	const TObjectPtr<AActor>* Found = ById.Find(Id);
	return Found ? Found->Get() : nullptr;
}

void UDarcMarkupSubsystem::Warn(const FDarcMarkup& Markup, const FString& Message) const
{
	UE_LOG(LogDarcMarkup, Warning, TEXT("DARC markup: %s (%s): %s"),
		Markup.Source.IsValid() ? *Markup.Source->GetActorNameOrLabel() : TEXT("?"), *Markup.Type.ToString(), *Message);
}

template <class T>
T* UDarcMarkupSubsystem::Ref(const FDarcMarkup& Markup, const TCHAR* Key) const
{
	const FName Id = Markup.GetName(Key);
	if (Id.IsNone())
	{
		return nullptr;
	}
	T* Found = Cast<T>(FindById(Id));
	if (!Found)
	{
		Warn(Markup, FString::Printf(TEXT("%s=%s: object with this Id not found or wrong type"), Key, *Id.ToString()));
	}
	return Found;
}

// ---------------------------------------------------------------------------
// Создание (сервер)
// ---------------------------------------------------------------------------

void UDarcMarkupSubsystem::ConvertMarkup()
{
	if (bConverted)
	{
		return;
	}
	bConverted = true;

	TArray<FDarcMarkup> All;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		FDarcMarkup Markup;
		if (ParseMarkup(*It, Markup) && !DarcMarkup::IsLocalOnly(Markup.Type))
		{
			All.Add(MoveTemp(Markup));
		}
	}
	if (All.Num() == 0)
	{
		return;
	}
	All.StableSort([](const FDarcMarkup& A, const FDarcMarkup& B) { return DarcMarkup::Priority(A.Type) < DarcMarkup::Priority(B.Type); });

	// 1) Создать (отложенно) и запомнить по Id; 2) связать ссылки; 3) достроить в том же порядке.
	struct FSpawned { AActor* Actor; const FDarcMarkup* Markup; FTransform Transform; };
	TArray<FSpawned> Spawned;
	for (const FDarcMarkup& Markup : All)
	{
		FTransform Transform;
		AActor* Actor = SpawnFor(Markup, Transform);
		if (!Actor)
		{
			continue;
		}
		const FName Id = Markup.GetName(TEXT("Id"));
		if (!Id.IsNone())
		{
			if (ById.Contains(Id))
			{
				Warn(Markup, FString::Printf(TEXT("duplicate Id=%s"), *Id.ToString()));
			}
			ById.Add(Id, Actor);
		}
		if (Actor != Markup.Source.Get())
		{
			Spawned.Add({ Actor, &Markup, Transform });
		}
	}
	for (const FSpawned& Entry : Spawned)
	{
		Configure(Entry.Actor, *Entry.Markup);
	}
	for (const FSpawned& Entry : Spawned)
	{
		Entry.Actor->FinishSpawning(Entry.Transform);
		// ВРЕМЕННО: название/подсказка под прицелом (теги Name=Ключ, Hint=Ключ из ST_UI).
		UDarcHintComponent::AddHint(Entry.Actor, Entry.Markup->GetName(TEXT("Name")), Entry.Markup->GetName(TEXT("Hint")));
	}
	UE_LOG(LogDarcMarkup, Log, TEXT("DARC markup: %d tagged actors, %d gameplay objects created"), All.Num(), Spawned.Num());
}

AActor* UDarcMarkupSubsystem::SpawnFor(const FDarcMarkup& Markup, FTransform& Transform)
{
	AActor* Source = Markup.Source.Get();
	if (!Source)
	{
		return nullptr;
	}
	const FName Type = Markup.Type;
	auto Is = [Type](const TCHAR* Name) { return Type.IsEqual(Name, ENameCase::IgnoreCase); };

	// Точка — сам исходный актор (цель доставки и т.п.), ничего не создаём.
	if (Is(TEXT("Point")))
	{
		return Source;
	}

	UClass* Class = nullptr;
	if (Is(TEXT("Door")))                                  Class = AInteractableDoor::StaticClass();
	else if (Is(TEXT("Lamp")))                             Class = ADarcPowerLamp::StaticClass();
	else if (Is(TEXT("FuseBox")))                          Class = ADarcFuseBox::StaticClass();
	else if (Is(TEXT("Generator")))                        Class = ADarcGenerator::StaticClass();
	else if (Is(TEXT("Heavy")))                            Class = ADarcHeavyObject::StaticClass();
	else if (Is(TEXT("Inlet")))                            Class = ADarcPowerInlet::StaticClass();
	else if (Is(TEXT("Terminal")) || Is(TEXT("Keypad")))   Class = ADarcTerminal::StaticClass();
	else if (Is(TEXT("CardReader")))                       Class = ADarcCardReader::StaticClass();
	else if (Is(TEXT("Slot")))                             Class = ADarcItemSlot::StaticClass();
	else if (Is(TEXT("Station")))                          Class = ADarcDataTransferStation::StaticClass();
	else if (Is(TEXT("Npc")))                              Class = ADarcNpc::StaticClass();
	else if (Is(TEXT("Anchor")))                           Class = ADarcRareEventAnchor::StaticClass();
	else if (Is(TEXT("Room")))                             Class = ADarcRoomVolume::StaticClass();
	else if (Is(TEXT("Item")) || Is(TEXT("Keycard")) || Is(TEXT("Drive")) || Is(TEXT("Module")) || Is(TEXT("Fuse")))
	{
		Class = DarcMarkup::ItemClass(Type.ToString());
	}
	if (!Class)
	{
		Warn(Markup, TEXT("unknown type, left as is"));
		return nullptr;
	}

	Transform = Source->GetActorTransform();
	Transform.SetScale3D(FVector::OneVector); // масштаб модели переносит VisualSpec, не актор

	if (Is(TEXT("Door")))
	{
		// Петля — у края створки (Hinge=Left по умолчанию / Right / Pivot): дверь вращается вокруг неё.
		const UStaticMeshComponent* Mesh = Source->FindComponentByClass<UStaticMeshComponent>();
		const FString HingeMode = Markup.Get(TEXT("Hinge"), TEXT("Left"));
		if (Mesh && Mesh->GetStaticMesh() && !HingeMode.Equals(TEXT("Pivot"), ESearchCase::IgnoreCase))
		{
			const FBox Local = Mesh->GetStaticMesh()->GetBoundingBox();
			const FVector Size = Local.GetSize() * Mesh->GetComponentScale().GetAbs();
			const bool bAlongX = Size.X >= Size.Y; // ширина створки — по большей горизонтальной оси
			const bool bRight = HingeMode.Equals(TEXT("Right"), ESearchCase::IgnoreCase);
			const FVector Center = Local.GetCenter();
			FVector HingeLocal(Center.X, Center.Y, Local.Min.Z);
			if (bAlongX)
			{
				HingeLocal.X = bRight ? Local.Max.X : Local.Min.X;
			}
			else
			{
				HingeLocal.Y = bRight ? Local.Max.Y : Local.Min.Y;
			}
			Transform.SetLocation(Mesh->GetComponentTransform().TransformPosition(HingeLocal));
			Transform.SetRotation(Mesh->GetComponentQuat());
		}
	}
	else if (Is(TEXT("Room")))
	{
		FVector Origin, Extent;
		Source->GetActorBounds(false, Origin, Extent);
		Transform = FTransform(Origin);
	}

	return GetWorld()->SpawnActorDeferred<AActor>(Class, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
}

void UDarcMarkupSubsystem::Configure(AActor* Actor, const FDarcMarkup& M)
{
	using namespace DarcMarkup;
	AActor* Source = M.Source.Get();
	const FName Id = M.GetName(TEXT("Id"));

	if (ACarryableItem* Item = Cast<ACarryableItem>(Actor))
	{
		Item->MemoryId = Id;
		Item->PromptPickUp = Txt(TEXT("Carry_PickUp"));
		Item->PromptDrop = Txt(TEXT("Carry_Drop"));
		Item->VisualSpec = CopyOf(Source);
		if (ADarcKeycard* Card = Cast<ADarcKeycard>(Item))
		{
			Card->BackMarking->CodeKey = M.GetName(TEXT("Code"), TEXT("CatalogCode"));
		}
	}
	else if (AInteractableDoor* Door = Cast<AInteractableDoor>(Actor))
	{
		Door->MemoryId = Id;
		Door->bIsLocked = M.Has(TEXT("Locked"));
		Door->bIsOpen = M.Has(TEXT("Open"));
		Door->OpenAngle = M.GetFloat(TEXT("Angle"), Door->OpenAngle);
		Door->PromptOpen = Txt(TEXT("Door_Open"));
		Door->PromptClose = Txt(TEXT("Door_Close"));
		Door->PromptLocked = Txt(TEXT("Door_Locked"));
		Door->VisualSpec = CopyOf(Source);
	}
	else if (ADarcPowerLamp* Lamp = Cast<ADarcPowerLamp>(Actor))
	{
		Lamp->Power->CircuitId = M.GetName(TEXT("Circuit"), TEXT("Building"));
		Lamp->VisualSpec = CopyOf(Source);
	}
	else if (ADarcFuseBox* Box = Cast<ADarcFuseBox>(Actor))
	{
		Box->CircuitId = M.GetName(TEXT("Circuit"));
		if (Box->CircuitId.IsNone())
		{
			Warn(M, TEXT("FuseBox without Circuit="));
		}
		Box->bHasFuse = !M.Has(TEXT("NoFuse"));
		Box->bBreakerOn = !M.Has(TEXT("Off"));
		for (const FString& Circuit : M.GetList(TEXT("Collateral")))
		{
			Box->CollateralCircuits.Add(FName(*Circuit));
		}
		const FName BlowTask = M.GetName(TEXT("TaskOnBlow"));
		if (!BlowTask.IsNone())
		{
			Box->bAddTaskOnBlow = true;
			Box->TaskOnBlow.TaskId = BlowTask;
			Box->TaskOnBlow.Title = Txt(M.Get(TEXT("TaskTitle"), TEXT("Task_") + BlowTask.ToString()));
			Box->TaskOnBlow.bRequired = false;
		}
		Box->PromptInsertFuse = Txt(TEXT("Fuse_Insert"));
		Box->PromptBreakerOn = Txt(TEXT("Breaker_On"));
		Box->PromptBreakerOff = Txt(TEXT("Breaker_Off"));
		Box->VisualSpec = CopyOf(Source);
	}
	else if (ADarcHeavyObject* Heavy = Cast<ADarcHeavyObject>(Actor))
	{
		// Генератор — наследник тяжёлого объекта: общие настройки.
		Heavy->MassKg = M.GetFloat(TEXT("Mass"), Heavy->MassKg);
		if (M.Has(TEXT("NoCable")))
		{
			Heavy->bHasFragileCable = false;
		}
		Heavy->DeliveryTarget = Ref<AActor>(M, TEXT("Deliver"));
		Heavy->DeliveryRadius = M.GetFloat(TEXT("Radius"), Heavy->DeliveryRadius);
		Heavy->TaskIdOnDelivered = M.GetName(TEXT("Task"));
		Heavy->PromptReattachCable = Txt(TEXT("Heavy_ReattachCable"));
		Heavy->VisualSpec = CopyOf(Source);
	}
	else if (ADarcPowerInlet* Inlet = Cast<ADarcPowerInlet>(Actor))
	{
		Inlet->CircuitId = M.GetName(TEXT("Circuit"));
		Inlet->CableLength = M.GetFloat(TEXT("Cable"), Inlet->CableLength);
		Inlet->PromptConnect = Txt(TEXT("Inlet_Connect"));
		Inlet->PromptDisconnect = Txt(TEXT("Inlet_Disconnect"));
		Inlet->VisualSpec = CopyOf(Source);
	}
	else if (ADarcTerminal* Terminal = Cast<ADarcTerminal>(Actor))
	{
		const bool bKeypad = M.Type.IsEqual(TEXT("Keypad"), ENameCase::IgnoreCase);
		Terminal->CodeKey = M.GetName(TEXT("Code"), bKeypad ? FName(TEXT("ServerDoorCode")) : FName(TEXT("CatalogCode")));
		Terminal->FixedCode = M.Get(TEXT("FixedCode"));
		Terminal->bStartsUnlocked = M.Has(TEXT("Unlocked"));
		Terminal->TaskIdOnUnlock = M.GetName(TEXT("Task"));
		Terminal->DoorToUnlock = Ref<AInteractableDoor>(M, TEXT("Door"));
		Terminal->Power->CircuitId = M.GetName(TEXT("Circuit"));
		Terminal->LockedTitle = Txt(M.Get(TEXT("LockedTitle"), TEXT("Screen_EnterCode")));
		Terminal->UnlockedTitle = Txt(M.Get(TEXT("Title"), bKeypad ? TEXT("Screen_AccessGranted") : TEXT("Screen_CatalogTitle")));
		Terminal->ScrambledWords = Texts(M.GetList(TEXT("Words")));
		Terminal->ScreenLines = Texts(M.GetList(TEXT("Lines")));
		// Commands=COPY:TakeExtraFile,DELETE:SomeTask — команда → задача.
		for (const FString& Pair : M.GetList(TEXT("Commands")))
		{
			FString Command, Task;
			if (Pair.Split(TEXT(":"), &Command, &Task))
			{
				Terminal->CommandTasks.Add(Command.ToUpper(), FName(*Task));
			}
		}
		Terminal->PromptUse = Txt(TEXT("Terminal_Use"));
		Terminal->PromptBusy = Txt(TEXT("Terminal_Busy"));
		Terminal->PromptNoPower = Txt(TEXT("Terminal_NoPower"));
		Terminal->VisualSpec = CopyOf(Source);
	}
	else if (ADarcCardReader* Reader = Cast<ADarcCardReader>(Actor))
	{
		Reader->AcceptedClass = ItemClass(M.Get(TEXT("Accepts"), TEXT("Keycard")));
		Reader->DoorToUnlock = Ref<AInteractableDoor>(M, TEXT("Door"));
		Reader->TaskIdOnAccess = M.GetName(TEXT("Task"));
		Reader->PromptSwipe = Txt(TEXT("Card_Swipe"));
		Reader->VisualSpec = CopyOf(Source);
	}
	else if (ADarcItemSlot* Slot = Cast<ADarcItemSlot>(Actor))
	{
		Slot->AcceptedClass = ItemClass(M.Get(TEXT("Accepts")));
		Slot->InitialItem = Ref<ACarryableItem>(M, TEXT("Holds"));
		Slot->bAllowRemove = !M.Has(TEXT("Fixed"));
		Slot->TaskIdOnInsert = M.GetName(TEXT("Task"));
		Slot->TaskIdOnRemove = M.GetName(TEXT("TaskRemove"));
		Slot->PromptInsert = Txt(TEXT("Slot_Insert"));
		Slot->PromptRemove = Txt(TEXT("Slot_Remove"));
		Slot->VisualSpec = CopyOf(Source);
	}
	else if (ADarcDataTransferStation* Station = Cast<ADarcDataTransferStation>(Actor))
	{
		Station->InterfaceSlot = Ref<ADarcItemSlot>(M, TEXT("Interface"));
		Station->DriveSlot = Ref<ADarcItemSlot>(M, TEXT("DriveSlot"));
		Station->TaskIdOnComplete = M.GetName(TEXT("Task"));
		Station->Power->CircuitId = M.GetName(TEXT("Circuit"), Station->Power->CircuitId);
		Station->AnomalyFlickerCircuit = M.GetName(TEXT("Flicker"), Station->AnomalyFlickerCircuit);
		Station->TransferSeconds = M.GetFloat(TEXT("Seconds"), Station->TransferSeconds);
		Station->PromptStart = Txt(TEXT("Transfer_Start"));
		Station->VisualSpec = CopyOf(Source);
	}
	else if (ADarcNpc* Npc = Cast<ADarcNpc>(Actor))
	{
		Npc->SpeakerName = Txt(M.Get(TEXT("Speaker"), TEXT("Speaker_Guard")));
		Npc->GreetingLines = Lines(M.GetList(TEXT("Greeting")));
		Npc->ItemToGive = Ref<ACarryableItem>(M, TEXT("Gives"));
		Npc->TaskIdOnGive = M.GetName(TEXT("TaskGive"));
		Npc->GiveLines = Lines(M.GetList(TEXT("GiveLines")), 0.2f);
		Npc->AcceptedClass = ItemClass(M.Get(TEXT("Accepts")));
		Npc->BeforeReceiveLines = Lines(M.GetList(TEXT("BeforeReceive")), 0.2f);
		Npc->TaskIdOnReceive = M.GetName(TEXT("TaskReceive"));
		Npc->ReceiveLines = Lines(M.GetList(TEXT("ReceiveLines")));
		Npc->IdleLines = Lines(M.GetList(TEXT("Idle")));
		Npc->PromptTalk = Txt(TEXT("Npc_Talk"));
		Npc->PromptHandOver = Txt(TEXT("Npc_HandOver"));
		Npc->VisualSpec = CopyOf(Source);
	}
	else if (ADarcRareEventAnchor* Anchor = Cast<ADarcRareEventAnchor>(Actor))
	{
		for (const FString& Event : M.GetList(TEXT("Events")))
		{
			Anchor->SupportedEventTypes.Add(FName(*Event));
		}
		Anchor->RoomId = M.GetName(TEXT("Room"));
		const FString Behavior = M.Get(TEXT("Behavior"));
		if (Behavior.Equals(TEXT("LightOn"), ESearchCase::IgnoreCase))            Anchor->Behavior = EDarcAnchorBehavior::LightOn;
		else if (Behavior.Equals(TEXT("Levitate"), ESearchCase::IgnoreCase))      Anchor->Behavior = EDarcAnchorBehavior::Levitate;
		else if (Behavior.Equals(TEXT("VanishingRoom"), ESearchCase::IgnoreCase)) Anchor->Behavior = EDarcAnchorBehavior::VanishingRoom;
		Anchor->TargetActor = Ref<AActor>(M, TEXT("Target"));
		Anchor->TargetTag = M.GetName(TEXT("TargetTag"));
		Anchor->RoomDoor = Ref<AInteractableDoor>(M, TEXT("Door"));
		Anchor->EffectDuration = M.GetFloat(TEXT("Duration"), Anchor->EffectDuration);
		Anchor->VisualSpec = CopyOf(Source); // нет модели (TargetPoint) — якорь невидимый
	}
	else if (ADarcRoomVolume* Room = Cast<ADarcRoomVolume>(Actor))
	{
		Room->RoomId = Id;
		if (Id.IsNone())
		{
			Warn(M, TEXT("Room without Id="));
		}
		FVector Origin, Extent;
		Source->GetActorBounds(false, Origin, Extent);
		Room->Bounds->SetBoxExtent(Extent.ComponentMax(FVector(10.f)));
	}
}

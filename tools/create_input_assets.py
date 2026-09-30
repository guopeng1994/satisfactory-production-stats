# SPDX-License-Identifier: 0BSD
"""Run in the locked Unreal Editor's Python console; creates genuine input assets.
Does not build, cook, package or publish. Re-running validates existing assets and
keeps their authored settings. Enable Python Editor Script Plugin beforehand.
"""
import unreal
from pathlib import Path

ROOT = '/FactoryProductionStats/Inputs'
ACTION_PATH = ROOT + '/IA_ProductionStats'
CONTEXT_PATH = ROOT + '/MC_ProductionStats'
PARENT_PATH = '/Game/FactoryGame/Inputs/Player/MC_PlayerActions'
UI_PARENT_PATH = '/Game/FactoryGame/Interface/UI/Inputs/MC_UserInterfaceBase'


def make_asset(path, asset_class):
    existing = unreal.EditorAssetLibrary.load_asset(path)
    if existing:
        if not isinstance(existing, asset_class):
            raise RuntimeError('Existing asset has wrong type: ' + path)
        return existing, False
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', asset_class)
    folder, name = path.rsplit('/', 1)
    result = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, asset_class, factory)
    if not result:
        raise RuntimeError('Failed to create: ' + path)
    return result, True


parent = unreal.EditorAssetLibrary.load_asset(PARENT_PATH)
if not parent:
    raise RuntimeError('Locked Starter Project parent context missing: ' + PARENT_PATH)
action, new_action = make_asset(ACTION_PATH, unreal.InputAction)
if new_action:
    action.set_editor_property('value_type', unreal.InputActionValueType.BOOLEAN)
    action.set_editor_property('consume_input', False)
    action.set_editor_property('trigger_when_paused', False)
    settings = unreal.PlayerMappableKeySettings(outer=action)
    settings.set_editor_property('name', 'FactoryProductionStats_Toggle')
    settings.set_editor_property('display_name', unreal.Text('打开／关闭生产统计'))
    settings.set_editor_property('display_category', unreal.Text('Factory Production Stats'))
    action.set_editor_property('player_mappable_key_settings', settings)
contexts = []
for path, parent_path in ((CONTEXT_PATH, PARENT_PATH), (ROOT + '/MC_ProductionStats_UI', UI_PARENT_PATH)):
    context_parent = unreal.EditorAssetLibrary.load_asset(parent_path)
    if not context_parent:
        raise RuntimeError('Parent context missing: ' + parent_path)
    context, new_context = make_asset(path, unreal.FGChildInputMappingContext)
    if new_context:
        context.set_editor_property('m_parent_context', context_parent)
        context.set_editor_property('m_display_name', unreal.Text('Factory Production Stats'))
        context.set_editor_property('m_menu_priority', 200.0)
        context.set_editor_property('mappings', [unreal.EnhancedActionKeyMapping(action=action, key=unreal.Key('P'))])
    if context.get_editor_property('m_parent_context') != context_parent:
        raise RuntimeError('Existing context has another parent: ' + path)
    if not any(m.get_editor_property('action') == action for m in context.get_editor_property('mappings')):
        raise RuntimeError('Existing context does not map the statistics action: ' + path)
    contexts.append(path)
settings = action.get_editor_property('player_mappable_key_settings')
if not settings or str(settings.get_editor_property('name')) != 'FactoryProductionStats_Toggle':
    raise RuntimeError('Mapping name must be FactoryProductionStats_Toggle; inspect existing action')
for path in [ACTION_PATH] + contexts:
    if not unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False):
        raise RuntimeError('Failed to save asset: ' + path)
# String-only runtime LoadObject paths are not cooker references. Make inclusion
# explicit in the Starter project config, preserving all existing settings.
config = Path(unreal.Paths.project_config_dir()) / 'DefaultGame.ini'
text = config.read_text(encoding='utf-8-sig') if config.exists() else ''
entry = '+DirectoriesToAlwaysCook=(Path="/FactoryProductionStats/Inputs")'
if entry not in text:
    text += '\n[/Script/UnrealEd.ProjectPackagingSettings]\n' + entry + '\n'
    config.parent.mkdir(parents=True, exist_ok=True)
    config.write_text(text, encoding='utf-8')
unreal.log('Real T06 input assets saved; Inputs added to project cook directories. Verify discovery/rebinding and cooked package on Windows.')

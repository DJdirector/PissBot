# Role Menus

Module id: `roles`. Post a message with buttons. Members click a button to get a role, and click again to remove it.

## Creating a menu

Needs the **Role menus** [permission](Permissions) (administrators by default).

```
/rolemenu title:Pick your colour role1:@Red role2:@Blue role3:@Green
```

- `title` is the text above the buttons.
- `role1` is required. `role2` to `role5` are optional, up to 5 roles per menu.
- The menu is posted publicly in the current channel. Run the command again for another menu.

## How it behaves

- Each button is labelled with the role's name. Clicking it adds the role, or removes it if the member already has it. The reply is private.
- Menus keep working after bot restarts. Nothing about them is stored in the database.
- To change a menu, post a new one and delete the old message.
- Roles with the **Administrator** permission cannot be put in a menu.
- If a role changes, the button still points to the original role. If you delete a role, its button stops working.

## Requirements

The bot needs **Manage Roles** and its role must sit above every role in the menu. If it cannot change a role, the member gets a short message saying so.

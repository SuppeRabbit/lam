<?
/*
$Id$

  This code is part of LDAP Account Manager (http://www.sourceforge.net/projects/lam)
  Copyright (C) 2003  Tilo Lutz

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


  LDAP Account Manager Delete user, hosts or groups
*/
include_once('../lib/ldap.inc');
include_once('../lib/config.inc');
session_save_path('../sess');
@session_start();


echo '<html><head><title>';
echo _('Delete Account');
echo '</title>
	</head><body>
	<link rel="stylesheet" type="text/css" href="../style/layout.css">
	<form action="delete.php" method="post">
	<meta http-equiv="pragma" content="no-cache">
	<meta http-equiv="cache-control" content="no-cache">
	<table rules="all" class="delete" width="100%">
	<tr><td>';

if ($_GET['type']) {
	$DN2 = explode(";", str_replace("\'", '',$_GET['DN']));
	echo '<input name="type5" type="hidden" value="'.$_GET['type'].'">';
	echo '<input name="DN" type="hidden" value="'.$_GET['DN'].'">';
	switch ($_GET['type']) {
		case 'user':
			echo _('Do you really want to delete user(s):');
			break;
		case 'host':
			echo _('Do you really want to delete host(s):');
			break;
		case 'group':
			echo _('Do you really want to delete group(s):');
			break;
		}
	echo '</td></tr>';
	foreach ($DN2 as $dn) echo '<tr><td>'.$dn.'</td></tr>';
	echo '<br><tr><td>
	<input name="delete_yes" type="submit" value="';
	echo _('Commit'); echo '"></td><td></td><td>
	<input name="delete_no" type="submit" value="';
	echo _('Cancel'); echo '">';
	}

if ($_POST['delete_yes']) {
	$DN2 = explode(";", str_replace("\\", '',str_replace("\'", '',$_POST['DN'])));
	foreach ($DN2 as $dn) {
		switch ($_POST['type5']) {
			case 'user':
				$success = ldap_delete($_SESSION['ldap']->server(), $dn);
				if (!$success) $error = _('Could not delete user: ').$dn;
				break;
			case 'host':
				$success = ldap_delete($_SESSION['ldap']->server(), $dn);
				if (!$success) $error = _('Could not delete host: ').$dn;
				break;
			case 'group':
				$result = ldap_search($_SESSION['ldap']->server(), $dn, 'objectClass=*');
				if (!$result) $error = _('Could not delete group: ').$dn;
				$entry = ldap_first_entry($_SESSION['ldap']->server(), $result);
				$attr = ldap_get_attributes($_SESSION['ldap']->server(), $entry);
				if ($attr['memberUid']) $error = _('Could not delete group. Still users in group: ').$dn;
				    else {
					$success = ldap_delete($_SESSION['ldap']->server(), $dn);
					if (!$success) $error = _('Could not delete user: ').$dn;
					}
				break;
			}
		if (!$error) echo $dn. _(' deleted.');
		 else echo $error;
		echo '</td></tr><tr><td>';
		}
	}

if ($_POST['delete_no']) echo _('Nothing was deleted.');

echo '</td></tr>';
echo '</form></body></html>';
?>

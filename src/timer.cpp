// simplewall
// Copyright (c) 2016-2019 Henry++

#include "global.hpp"

void _app_timer_create (HWND hwnd, const MFILTER_APPS *ptr_apps, time_t seconds)
{
	if (!config.hengine || !ptr_apps || ptr_apps->empty ())
		return;

	const time_t current_time = _r_unixtime_now ();

	_r_fastlock_acquireexclusive (&lock_access);

	for (size_t i = 0; i < ptr_apps->size (); i++)
	{
		PITEM_APP ptr_app = ptr_apps->at (i);

		if (!ptr_app)
			continue;

		if (ptr_app->htimer)
		{
			if (DeleteTimerQueueTimer (config.htimer, ptr_app->htimer, nullptr))
				ptr_app->htimer = nullptr;
		}

		if (ptr_app->timer)
			ptr_app->timer = 0;

		const size_t hash = _r_str_hash (ptr_app->original_path); // note: be carefull (!)

		if (CreateTimerQueueTimer (&ptr_app->htimer, config.htimer, &_app_timer_callback, (PVOID)hash, DWORD (seconds * _R_SECONDSCLOCK_MSEC), 0, WT_EXECUTEONLYONCE | WT_EXECUTEINTIMERTHREAD))
		{
			ptr_app->is_enabled = true;
			ptr_app->timer = current_time + seconds;

			const size_t item = _app_getposition (hwnd, hash);

			if (item != LAST_VALUE)
			{
				_r_fastlock_acquireshared (&lock_checkbox);

				_r_listview_setitem (hwnd, IDC_APPS_PROFILE, item, 0, nullptr, LAST_VALUE, _app_getappgroup (hash, ptr_app));
				_r_listview_setitemcheck (hwnd, IDC_APPS_PROFILE, item, ptr_app->is_enabled);

				_r_fastlock_releaseshared (&lock_checkbox);
			}
		}
	}

	_r_fastlock_releaseexclusive (&lock_access);

	_wfp_create3filters (ptr_apps, __LINE__);
}

size_t _app_timer_remove (HWND hwnd, const MFILTER_APPS *ptr_apps)
{
	if (!config.hengine || !ptr_apps || ptr_apps->empty ())
		return false;

	const time_t current_time = _r_unixtime_now ();
	size_t count = 0;

	MARRAY ids;

	_r_fastlock_acquireexclusive (&lock_access);

	for (size_t i = 0; i < ptr_apps->size (); i++)
	{
		PITEM_APP ptr_app = ptr_apps->at (i);

		if (!ptr_app || !_app_istimeractive (ptr_app))
			continue;

		ids.insert (ids.end (), ptr_app->mfarr.begin (), ptr_app->mfarr.end ());
		ptr_app->mfarr.clear ();
		ptr_app->is_haveerrors = false;

		if (ptr_app->htimer)
		{
			if (DeleteTimerQueueTimer (config.htimer, ptr_app->htimer, nullptr))
				ptr_app->htimer = nullptr;
		}

		if(ptr_app->timer)
			ptr_app->timer = 0;

		ptr_app->is_enabled = false;

		const size_t app_hash = _r_str_hash (ptr_app->original_path); // note: be carefull (!)
		const size_t item = _app_getposition (hwnd, app_hash);
		const UINT listview_id = _app_getlistview_id (ptr_app->type);

		if (item != LAST_VALUE)
		{
			if (!_app_isappexists (ptr_app) || ptr_app->is_temp)
			{
				if (listview_id)
					SendDlgItemMessage (hwnd, listview_id, LVM_DELETEITEM, item, 0);

				_app_freeapplication (app_hash);
			}
			else
			{
				if (listview_id)
				{
					_r_fastlock_acquireshared (&lock_checkbox);

					_r_listview_setitem (hwnd, listview_id, item, 0, nullptr, LAST_VALUE, _app_getappgroup (app_hash, ptr_app));
					_r_listview_setitemcheck (hwnd, listview_id, item, ptr_app->is_enabled);

					_r_fastlock_releaseshared (&lock_checkbox);
				}
			}
		}

		count += 1;
	}

	_r_fastlock_releaseexclusive (&lock_access);

	_wfp_destroy2filters (&ids, __LINE__);

	return count;
}

bool _app_istimeractive (ITEM_APP const *ptr_app)
{
	return ptr_app->htimer || (ptr_app->timer && (ptr_app->timer > _r_unixtime_now ()));
}

bool _app_istimersactive ()
{
	_r_fastlock_acquireshared (&lock_access);

	for (auto &p : apps)
	{
		if (_app_istimeractive (&p.second))
		{
			_r_fastlock_releaseshared (&lock_access);
			return true;
		}
	}

	_r_fastlock_releaseshared (&lock_access);

	return false;
}

void CALLBACK _app_timer_callback (PVOID lparam, BOOLEAN)
{
	_r_fastlock_acquireshared (&lock_access);

	const size_t app_hash = (size_t)lparam;
	const PITEM_APP ptr_app = _app_getapplication (app_hash);
	const HWND hwnd = app.GetHWND ();

	MFILTER_APPS rules;
	rules.push_back (ptr_app);

	_r_fastlock_releaseshared (&lock_access);

	const bool is_succcess = _app_timer_remove (hwnd, &rules);

	if (is_succcess)
	{
		const UINT listview_id = _app_gettab_id (hwnd);

		_app_listviewsort (hwnd, listview_id);
		_app_profile_save ();

		_r_listview_redraw (hwnd, listview_id);

		if (app.ConfigGet (L"IsNotificationsTimer", true).AsBool ())
			app.TrayPopup (hwnd, UID, nullptr, NIIF_USER | (app.ConfigGet (L"IsNotificationsSound", true).AsBool () ? 0 : NIIF_NOSOUND), APP_NAME, _r_fmt (app.LocaleString (IDS_STATUS_TIMER_DONE, nullptr), ptr_app->display_name));
	}
}

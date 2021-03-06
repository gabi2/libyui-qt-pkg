/**************************************************************************
Copyright (C) 2000 - 2010 Novell, Inc.
All Rights Reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

**************************************************************************/


/*---------------------------------------------------------------------\
|								       |
|		       __   __	  ____ _____ ____		       |
|		       \ \ / /_ _/ ___|_   _|___ \		       |
|			\ V / _` \___ \ | |   __) |		       |
|			 | | (_| |___) || |  / __/		       |
|			 |_|\__,_|____/ |_| |_____|		       |
|								       |
|				core system			       |
|							 (C) SuSE GmbH |
\----------------------------------------------------------------------/

  File:	      YQPkgRepoFilterView.h

  Author:     Stefan Hundhammer <sh@suse.de>

/-*/


#ifndef YQPkgRepoFilterView_h
#define YQPkgRepoFilterView_h

#include "YQZypp.h"
#include <QWidget>


class YQPkgRepoList;
class QY2ComboTabWidget;
class YQPkgRpmGroupTagsFilterView;
class YQPkgSearchFilterView;
class YQPkgStatusFilterView;


class YQPkgRepoFilterView : public QWidget
{
    Q_OBJECT

public:

    /**
     * Constructor
     **/
    YQPkgRepoFilterView( QWidget * parent );

    /**
     * Destructor
     **/
    virtual ~YQPkgRepoFilterView();

    /**
     * Current selected repository, or if nothing is selected
     */
    zypp::Repository selectedRepo() const;
    
signals:

    /**
     * Emitted when the filtering starts. Use this to clear package lists
     * etc. prior to adding new entries.
     **/
    void filterStart();

    /**
     * Emitted during filtering for each pkg that matches the filter
     * and the candidate package comes from the respective repository
     **/
    void filterMatch( ZyppSel	selectable,
		      ZyppPkg	pkg );

    /**
     * Emitted during filtering for each pkg that matches the filter
     * and the candidate package does not come from the respective repository
     **/
    void filterNearMatch( ZyppSel	selectable,
			  ZyppPkg	pkg );

    /**
     * Emitted when filtering is finished.
     **/
    void filterFinished();

public slots:

    /**
     * Filter according to the view's rules and current selection.
     * Emits those signals:
     *    filterStart()
     *    filterMatch() for each pkg that matches the filter
     *    filterFinished()
     **/
    void filter();

    /**
     * Same as filter(), but only if this widget is currently visible.
     **/
    void filterIfVisible();

    
protected slots:

    /**
     * Propagate a filter match from the primary filter
     * and appy any selected secondary filter(s) to it
     **/
    void primaryFilterMatch( ZyppSel	selectable,
			     ZyppPkg 	pkg );
    
    /**
     * Propagate a filter near match from the primary filter
     * and appy any selected secondary filter(s) to it
     **/
    void primaryFilterNearMatch( ZyppSel	selectable,
				 ZyppPkg	pkg );

protected:

    /**
     * Widget layout for the secondary filters
     **/
    QWidget * layoutSecondaryFilters( QWidget * parent );

    /**
     * Check if pkg matches the the currently selected secondary filter
     **/
    bool secondaryFilterMatch( ZyppSel 	selectable,
			       ZyppPkg 		pkg );


    // Data members

    YQPkgRepoList *		_repoList;
    QY2ComboTabWidget *		_secondaryFilters;
    QWidget *			    _allPackages;
    QWidget *_unmaintainedPackages;
    YQPkgRpmGroupTagsFilterView *   _rpmGroupTagsFilterView;
    YQPkgSearchFilterView *	    _searchFilterView;
    YQPkgStatusFilterView *	    _statusFilterView;
};



#endif // ifndef YQPkgRepoFilterView_h

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

  File:	      YQPkgPatchList.cc

  Author:     Stefan Hundhammer <sh@suse.de>

  Textdomain "qt-pkg"

/-*/

#define YUILogComponent "qt-pkg"
#include "YUILog.h"

#include <QPainter>
#include <QItemDelegate>
#include <QMenu>
#include <QAction>
#include <QEvent>
#include <QHeaderView>
#include <zypp/base/Logger.h>
#include <zypp/ui/PatchContents.h>
#include <set>

#include "YQi18n.h"
#include "utf8.h"

#include "YQPkgPatchList.h"
#include "YQPkgTextDialog.h"
#include "YQIconPool.h"

#define VERBOSE_PATCH_LIST	1


typedef zypp::ui::PatchContents			ZyppPatchContents;
typedef zypp::ui::PatchContents::const_iterator ZyppPatchContentsIterator;

using std::list;
using std::set;

class YQPkgPatchItemDelegate : public QItemDelegate
{
     YQPkgPatchList *_view;
public:
    YQPkgPatchItemDelegate( YQPkgPatchList *parent ) : QItemDelegate( parent ), _view( parent ) {
    }

    virtual void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const
    {
        painter->save();
        QColor background = option.palette.color(QPalette::Window);
        
        YQPkgPatchCategoryItem *citem = dynamic_cast<YQPkgPatchCategoryItem *>(_view->itemFromIndex(index));
        // special painting for category items
        if ( citem )
        {
            //std::cout << "printing category: " << index.column() << std::endl;
            QFont f = painter->font();
            f.setWeight(QFont::Bold);
            f.setPointSize(f.pointSize()+1);
            citem->setFont(_view->summaryCol(), f);
           
            painter->fillRect(option.rect, option.palette.color(QPalette::AlternateBase));
            QItemDelegate::paint(painter, option, index);
            painter->restore();
            return;
        }

        YQPkgPatchListItem *item = dynamic_cast<YQPkgPatchListItem *>(_view->itemFromIndex(index));
        if ( item )
        {
                painter->restore();
                QItemDelegate::paint(painter, option, index);
        }
    }
};


YQPkgPatchList::YQPkgPatchList( QWidget * parent )
    : YQPkgObjList( parent )
{
    yuiDebug() << "Creating patch list" << endl;

    _filterCriteria = RelevantPatches;

    int numCol = 0;

    QStringList headers;

    headers << "";			_statusCol	= numCol++;
    //headers <<  _( "Patch"	);	_nameCol	= numCol++;
    headers << _( "Summary" 	);	_summaryCol	= numCol++;
    //headers << _( "Category"	);	_categoryCol	= numCol++;
    //headers << _( "Size"	);	_sizeCol	= numCol++;
    //headers << _( "Version"	);	_versionCol	= numCol++;

    // Can use the same colum for "broken" and "satisfied":
    // Both states are mutually exclusive

    _satisfiedIconCol	= _summaryCol;
    _brokenIconCol	= _summaryCol;

    setHeaderLabels(headers);
    header()->setResizeMode(_statusCol, QHeaderView::ResizeToContents);
    //header()->setResizeMode(_versionCol, QHeaderView::ResizeToContents);
    header()->setResizeMode(_categoryCol, QHeaderView::ResizeToContents);
    header()->setResizeMode(_summaryCol, QHeaderView::Interactive);


    setItemDelegateForColumn( _summaryCol, new YQPkgPatchItemDelegate( this ) );
    setItemDelegateForColumn( _statusCol, new YQPkgPatchItemDelegate( this ) );

    setAllColumnsShowFocus( true );
    //FIXME setColumnAlignment( sizeCol(), Qt::AlignRight );

    connect( this,	SIGNAL( currentItemChanged	( QTreeWidgetItem *, QTreeWidgetItem* ) ),
	     this,	SLOT  ( filter()				    ) );

    sortItems( categoryCol(), Qt::AscendingOrder );
    fillList();

    yuiDebug() << "Creating patch list done" << endl;
}


YQPkgPatchList::~YQPkgPatchList()
{
    // NOP
}


void
YQPkgPatchList::polish()
{
    // Delayed initialization after widget is fully created etc.

    // Only now send currentItemChanged() signal so attached details views also
    // display something if their showDetailsIfVisible() slot is connected to
    // currentItemChanged() signals.
    selectSomething();
}

YQPkgPatchCategoryItem *
YQPkgPatchList::category( YQPkgPatchCategory category )
{
    YQPkgPatchCategoryItem * cat = _categories[ category ];

    if ( ! cat )
    {
        yuiDebug() << "New patch category \""<< category << "\"" << endl;

        cat = new YQPkgPatchCategoryItem( category, this );
        Q_CHECK_PTR( cat );
        _categories.insert( category, cat );
    }

    return cat;
}

void
YQPkgPatchList::setFilterCriteria( FilterCriteria filterCriteria )
{
    _filterCriteria = filterCriteria;
}


void
YQPkgPatchList::fillList()
{
    clear();
    yuiDebug() << "Filling patch list" << endl;

    for ( ZyppPoolIterator it = zyppPatchesBegin();
	  it != zyppPatchesEnd();
	  ++it )
    {
        ZyppSel	  selectable = *it;
        ZyppPatch zyppPatch = tryCastToZyppPatch( selectable->theObj() );

        if ( zyppPatch )
        {
            bool displayPatch = false;

            switch ( _filterCriteria )
            {
            case RelevantPatches:	// needed + broken + satisfied (but not installed)
                
                if ( selectable->hasInstalledObj() ) // installed?
                {
                    if ( selectable->installedPoolItem().status().isInstalled()
                         && selectable->installedPoolItem().isBroken() ) // patch broken?
                    {
                        // The patch is broken: It had been installed, but the user somehow
                        // downgraded individual packages belonging to the patch to older versions.
                        
                        displayPatch = true;
                        
                        yuiWarning() << "Installed patch is broken: "
                                     << zyppPatch->name()
                                     << " - " << zyppPatch->summary()
                                     << endl;
                    }
                }
                else // not installed
                {
                    if ( selectable->hasCandidateObj() &&
                         selectable->candidateObj().isSatisfied() )
                    {
                        // This is a pretty exotic case, but still it might happen:
                        //
                        // The patch itelf is not installed, but it is satisfied because the
                        // user updated all the packages belonging to the patch to the versions
                        // the patch requires. All that is missing now is to get the patch meta
                        // data onto the system. So let's display the patch to give the user
                        // a chance to install it (if he so chooses).
                        
                        displayPatch = true;
                        
                        yuiMilestone() << "Patch satisfied, but not installed yet: "
                                       << zyppPatch->name()
                                       << " - " << zyppPatch->summary()
                                       << endl;
                    }
                }
                
                if ( selectable->hasCandidateObj() )	// candidate available?
                {
                    // The most common case: There is a candidate patch, i.e. one that could be
                    // installed, but either no version of that patch is installed or there is a
                    // newer one to which the patch could be updated.
                    
                    if ( selectable->candidateObj().status().isUninstalled()
                         && selectable->candidateObj().isBroken() ) // patch really needed?
                    {
                        // Patches are needed if any of the packages that belong to the patch
                        // are installed on the system.
                        
                        displayPatch = true;
                    }
                    else
                    {
                        // None of the packages that belong to the patch is installed on the system.
                        
                        yuiDebug() << "Patch not needed: " << zyppPatch->name()
                                   << " - " << zyppPatch->summary()
                                   << endl;
                    }
                }
                break;
                
                
            case RelevantAndInstalledPatches:	// needed + broken + installed
                
                if ( selectable->hasInstalledObj() ) // installed?
                {
                    displayPatch = true;
                }
                else // not installed - display only if needed
                {
                    if ( selectable->candidateObj().isBroken() )
                    {
                        displayPatch = true;
                    }
                    else
                    {
                        yuiMilestone() << "Patch not needed: " << zyppPatch->name()
                                       << " - " << zyppPatch->summary()
                                       << endl;
                    }
                }
                break;
                
                
            case AllPatches:
                displayPatch = true;
                break;
                
                // Intentionally omitting "default" so the compiler
                // can catch unhandled enum values
            }
            
            if ( displayPatch )
            {
#if VERBOSE_PATCH_LIST
                yuiDebug() << "Displaying patch " << zyppPatch->name()
                           << " - " <<  zyppPatch->summary()
                           << endl;
#endif
                addPatchItem( *it, zyppPatch);
            }
        }
        else
        {
            yuiError() << "Found non-patch selectable" << endl;
        }
    }
    
#if FIXME
    if ( ! firstChild() )
        message( _( "No patches available." ) );
#endif
    
    yuiDebug() << "Patch list filled" << endl;
    resizeColumnToContents(_statusCol);
    //resizeColumnToContents(_nameCol);
    resizeColumnToContents(_categoryCol);
}



void
YQPkgPatchList::message( const QString & text )
{
    QY2ListViewItem * item = new QY2ListViewItem( this );
    Q_CHECK_PTR( item );

    item->setText( 1, text );
    item->setBackgroundColor( 0, QColor( 0xE0, 0xE0, 0xF8 ) );
}

void
YQPkgPatchList::filterIfVisible()
{
    if ( isVisible() )
	filter();
}


void
YQPkgPatchList::filter()
{
    emit filterStart();

    if ( selection() )
    {
        ZyppPatch patch = selection()->zyppPatch();

        if ( patch )
        {
            zypp::Patch::Contents c(patch->contents());
            MIL << c << endl;
            
            for ( zypp::Patch::Contents::Selectable_iterator it = c.selectableBegin();
                  it != c.selectableEnd();
                  ++it )
            {
                ZyppPkg zyppPkg = tryCastToZyppPkg( (*it)->theObj() );
                if ( zyppPkg )
                {
                    emit filterMatch( *it, zyppPkg );
                }
            }
        }
        else
        {
            MIL << "patch is bogus" << endl;
            
        }
        
  }
  else
      WAR << "selection empty" << endl;

  emit filterFinished();
}


void
YQPkgPatchList::addPatchItem( ZyppSel	selectable,
			      ZyppPatch zyppPatch )
{
    if ( ! selectable )
    {
        yuiError() << "NULL ZyppSel!" << endl;
        return;
    }

    YQPkgPatchCategoryItem * cat = category( YQPkgPatchCategoryItem::patchCategory(zyppPatch->category()));
    YQPkgPatchListItem * item = 0;

    if ( cat )
    {
        item = new YQPkgPatchListItem( this, cat, selectable, zyppPatch );
    }
    else
    {
        item = new YQPkgPatchListItem( this, selectable, zyppPatch );
    }
    applyExcludeRules( item );

}


YQPkgPatchListItem *
YQPkgPatchList::selection() const
{
    QTreeWidgetItem * item = currentItem();

    if ( ! item )
        return 0;

    return dynamic_cast<YQPkgPatchListItem *> (item);
}



void
YQPkgPatchList::createNotInstalledContextMenu()
{
    _notInstalledContextMenu = new QMenu( this );
    Q_CHECK_PTR( _notInstalledContextMenu );

    _notInstalledContextMenu->addAction(actionSetCurrentInstall);
    _notInstalledContextMenu->addAction(actionSetCurrentDontInstall);
    _notInstalledContextMenu->addAction(actionSetCurrentTaboo);

    addAllInListSubMenu( _notInstalledContextMenu );
}


void
YQPkgPatchList::createInstalledContextMenu()
{
    _installedContextMenu = new QMenu( this );
    Q_CHECK_PTR( _installedContextMenu );

    _installedContextMenu->addAction(actionSetCurrentKeepInstalled);

#if ENABLE_DELETING_PATCHES
    _installedContextMenu->addAction(actionSetCurrentDelete);
#endif

    _installedContextMenu->addAction(actionSetCurrentUpdate);
    _installedContextMenu->addAction(actionSetCurrentProtected);

    addAllInListSubMenu( _installedContextMenu );
}


QMenu *
YQPkgPatchList::addAllInListSubMenu( QMenu * menu )
{
    QMenu * submenu = new QMenu( menu );
    Q_CHECK_PTR( submenu );

    submenu->addAction(actionSetListInstall);
    submenu->addAction(actionSetListDontInstall);
    submenu->addAction(actionSetListKeepInstalled);

#if ENABLE_DELETING_PATCHES
    submenu->addAction(actionSetListDelete);
#endif

    submenu->addAction(actionSetListUpdate);
    submenu->addAction(actionSetListUpdateForce);
    submenu->addAction(actionSetListTaboo);
    submenu->addAction(actionSetListProtected);

    QAction *action = menu->addMenu(submenu);
    action->setText(_( "&All in This List" ));

    return submenu;
}


void
YQPkgPatchList::keyPressEvent( QKeyEvent * event )
{
    if ( event )
    {
#if ! ENABLE_DELETING_PATCHES
	if ( event->ascii() == '-' )
	{
	    QTreeWidgetItem * selectedListViewItem = currentItem();

	    if ( selectedListViewItem )
	    {
		YQPkgPatchListItem * item = dynamic_cast<YQPkgPatchListItem *> (selectedListViewItem);

		if ( item && item->selectable()->hasInstalledObj() )
		{
		    yuiWarning() << "Deleting patches is not supported" << endl;
		    return;
		}
	    }
	}
#endif
    }

    YQPkgObjList::keyPressEvent( event );
}


YQPkgPatchListItem::YQPkgPatchListItem( YQPkgPatchList *	patchList,
                                        YQPkgPatchCategoryItem *	parentCategory,
                                        ZyppSel			selectable,
                                        ZyppPatch		zyppPatch )
    : YQPkgObjListItem( patchList, parentCategory, selectable, zyppPatch )
    , _patchList( patchList )
    , _zyppPatch( zyppPatch )
{
     if ( ! _zyppPatch )
        _zyppPatch = tryCastToZyppPatch( selectable->theObj() );
    
    if ( ! _zyppPatch )
        return;

    init();
}

YQPkgPatchListItem::YQPkgPatchListItem( YQPkgPatchList *	patchList,
					ZyppSel			selectable,
					ZyppPatch		zyppPatch )
    : YQPkgObjListItem( patchList, selectable, zyppPatch )
    , _patchList( patchList )
    , _zyppPatch( zyppPatch )
{
     if ( ! _zyppPatch )
        _zyppPatch = tryCastToZyppPatch( selectable->theObj() );
    
    if ( ! _zyppPatch )
        return;

    init();
    
}

void YQPkgPatchListItem::init()
{   
    setStatusIcon();
    
    if ( summaryCol() > -1 && _zyppPatch->summary().empty() )
        setText( summaryCol(), _zyppPatch->name() );		// use name as fallback
}


YQPkgPatchListItem::~YQPkgPatchListItem()
{
    // NOP
}

void
YQPkgPatchListItem::cycleStatus()
{
    YQPkgObjListItem::cycleStatus();

    if ( status() == S_Del )	// Can't delete patches
        setStatus( S_KeepInstalled );
}


QString
YQPkgPatchListItem::toolTip( int col )
{
    QString text;

    if ( col == statusCol() )
    {
	text = YQPkgObjListItem::toolTip( col );
    }
    else
    {
	if (  ( col == brokenIconCol()	  && isBroken()	   ) ||
	      ( col == satisfiedIconCol() && isSatisfied() )   )
	{
	    text = YQPkgObjListItem::toolTip( col );
	}
	else
	{
	    text = fromUTF8( zyppPatch()->category() );

	    if ( ! text.isEmpty() )
		text += "\n";

	    text += fromUTF8( zyppPatch()->size().asString().c_str() );
	}
    }

    return text;
}


void
YQPkgPatchListItem::applyChanges()
{
    solveResolvableCollections();
}


bool YQPkgPatchListItem::operator< ( const QTreeWidgetItem & otherListViewItem ) const
{
    const YQPkgPatchListItem * other = dynamic_cast<const YQPkgPatchListItem *> (&otherListViewItem);
    int col = treeWidget()->sortColumn();
    if ( other )
    {
	if ( col == categoryCol() )
	{
	    return ( this->patchCategory() < other->patchCategory() );
	}
    }
    return YQPkgObjListItem::operator<( otherListViewItem );
}

YQPkgPatchCategoryItem::YQPkgPatchCategoryItem( YQPkgPatchCategory category, 
                                                YQPkgPatchList *	patchList )
    : QY2ListViewItem( patchList )
    , _patchList( patchList )
{

    _category = category;
    
    
    if ( _patchList->categoryCol() > -1 )
        setText( _patchList->summaryCol(), YQPkgPatchCategoryItem::asString( _category ) );
   

    //setText( _patchList->summaryCol(), "Category" );
    
    setExpanded( true );
    setTreeIcon();
}


YQPkgPatchCategory
YQPkgPatchCategoryItem::patchCategory( const string & category )
{
    return patchCategory( fromUTF8( category ) );
}


YQPkgPatchCategory
YQPkgPatchCategoryItem::patchCategory( QString category )
{
    category = category.toLower();

    if ( category == "yast"		) return YQPkgYaSTPatch;
    if ( category == "security"		) return YQPkgSecurityPatch;
    if ( category == "recommended"	) return YQPkgRecommendedPatch;
    if ( category == "optional"		) return YQPkgOptionalPatch;
    if ( category == "document"		) return YQPkgDocumentPatch;

    yuiWarning() << "Unknown patch category \"" << category << "\"" << endl;
    return YQPkgUnknownPatchCategory;
}


QString
YQPkgPatchCategoryItem::asString( YQPkgPatchCategory category )
{
    switch ( category )
    {
	// Translators: These are patch categories
	case YQPkgYaSTPatch:		return _( "YaST"	);
	case YQPkgSecurityPatch:	return _( "security"	);
	case YQPkgRecommendedPatch:	return _( "recommended" );
	case YQPkgOptionalPatch:	return _( "optional"	);
	case YQPkgDocumentPatch:	return _( "document"	);
	case YQPkgUnknownPatchCategory: return "";
    }

    return "";
}


YQPkgPatchCategoryItem::~YQPkgPatchCategoryItem()
{
    // NOP
}

void
YQPkgPatchCategoryItem::addPatch( ZyppPatch patch )
{
    if ( ! _firstPatch )
    {
        _firstPatch = patch;
    }
    else
    {
        //if ( _firstPatch->order().compare( pattern->order() ) < 0 )
        //    _firstPatch = pattern;
    }
}


void
YQPkgPatchCategoryItem::setExpanded( bool open )
{
    QTreeWidgetItem::setExpanded( open );
    setTreeIcon();
}


void
YQPkgPatchCategoryItem::setTreeIcon()
{
    setIcon( _patchList->iconCol(),
             isExpanded() ?
             YQIconPool::treeMinus() :
             YQIconPool::treePlus()   );

}


bool YQPkgPatchCategoryItem::operator< ( const QTreeWidgetItem & otherListViewItem ) const
{
    const YQPkgPatchCategoryItem * otherCategoryItem = dynamic_cast<const YQPkgPatchCategoryItem *>(&otherListViewItem);

    return category() < otherCategoryItem->category();
    return QTreeWidgetItem::operator<( otherListViewItem );
}



#include "YQPkgPatchList.moc"

use super::basic::Basic;
use super::Concrete;
use super::Desc;
use super::Layout;
use super::Staged;
use crate::rtree;
use crate::AbsRect;
use crate::URect;
use dyn_clone::DynClone;
use std::rc::Rc;

#[derive(Clone)]
pub struct Inherited {
    area: URect,
}

// The root node represents some area on the screen that contains a feather layout. Later this will turn
// into an absolute bounding volume. There can be multiple root nodes, each mapping to a different window.
#[derive(Clone)]
pub struct Root {
    area: AbsRect,
}

impl<AppData: 'static> Desc<AppData> for Root {
    type Props = Root;
    type Impose = Inherited;
    type Children<A: DynClone + ?Sized> = Box<dyn Layout<Self::Impose, AppData>>;

    fn stage(
        props: &Self::Props,
        _: AbsRect,
        child: &Self::Children<dyn Layout<Self::Impose, AppData>>,
    ) -> Box<dyn Staged<AppData>> {
        // We bypass creating our own node here as our staging node would be redundant.
        child.stage(child.get_imposed().area * props.area)
    }
}
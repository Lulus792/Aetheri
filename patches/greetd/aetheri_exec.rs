// SPDX-License-Identifier: GPL-3.0-only
// Literal argv for the installed Aetheri backend.
use std::{ffi::CString, path::Path};

use crate::error::Error;

pub(crate) fn arguments(command: &[String]) -> Result<Vec<CString>, Error> {
    if !command.first().is_some_and(|path| Path::new(path).is_absolute()) {
        return Err(Error::Error("session requires an absolute executable path".into()));
    }
    command
        .iter()
        .map(|argument| {
            CString::new(argument.as_bytes())
                .map_err(|_| Error::Error("session argument contains NUL".into()))
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn keeps_paths_and_arguments_literal() {
        let input = ["/absolute path/greeter", "", "space here", "$HOME; *", "\"quotes\""]
            .map(String::from);
        let output = arguments(&input).unwrap();
        assert_eq!(
            output.iter().map(|argument| argument.to_bytes()).collect::<Vec<_>>(),
            input.iter().map(|argument| argument.as_bytes()).collect::<Vec<_>>(),
        );
    }

    #[test]
    fn rejects_missing_relative_and_nul_arguments() {
        assert!(arguments(&[]).is_err());
        for input in [["", "x"], ["./relative", "x"], ["/bin/x\0y", ""], ["/bin/x", "\0"]] {
            assert!(arguments(&input.map(String::from)).is_err());
        }
    }
}

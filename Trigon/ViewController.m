// ViewController.m
// Trigon, 2025

#import "ViewController.h"

uint64_t trigon(void);

@interface ViewController ()
@end

@implementation ViewController

- (void)loadView {
    [super loadView];
    
    self.view = [[UIView alloc] initWithFrame:[UIScreen mainScreen].bounds];
    self.view.backgroundColor = [UIColor systemBackgroundColor];
    
    UILabel *label = [[UILabel alloc] initWithFrame:CGRectMake(0, 0, 300, 100)];
    label.center = self.view.center;
    label.textAlignment = NSTextAlignmentCenter;
    label.font = [UIFont systemFontOfSize:20 weight:UIFontWeightBold];
    label.numberOfLines = 2;
    
    uint64_t result = trigon();
    label.text = !result ? @"Exploit success!" : [NSString stringWithFormat:@"Exploit failed.\nResult: 0x%llX.", result];
    
    [self.view addSubview:label];
}

@end
